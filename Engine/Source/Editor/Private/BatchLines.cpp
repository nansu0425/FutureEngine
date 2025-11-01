#include "pch.h"
#include "Editor/Public/BatchLines.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Global/Octree.h"
#include "Component/Public/DecalSpotLightComponent.h"
#include "Physics/Public/OBB.h"

IMPLEMENT_CLASS(UBatchLines, UObject)

UBatchLines::UBatchLines() : Grid(), BoundingBoxLines()
{
	Vertices.reserve(Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices());
	Vertices.resize(Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices());

	Grid.MergeVerticesAt(Vertices, 0);
	BoundingBoxLines.MergeVerticesAt(Vertices, Grid.GetNumVertices());

	SetIndices();
	
	ID3D11VertexShader* VertexShader;
	ID3D11InputLayout* InputLayout;
	TArray<D3D11_INPUT_ELEMENT_DESC> Layout = { {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0} };
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/BatchLineShader.hlsl", Layout, &VertexShader, &InputLayout);
	ID3D11PixelShader* PixelShader;
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/BatchLineShader.hlsl", &PixelShader);
	
	Primitive.VertexShader = VertexShader;
	Primitive.InputLayout = InputLayout;
	Primitive.PixelShader = PixelShader;
	Primitive.NumVertices = static_cast<uint32>(Vertices.size());
	Primitive.NumIndices = static_cast<uint32>(Indices.size());
	Primitive.VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(Vertices.data(), Primitive.NumVertices * sizeof(FVector), true);
	Primitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(Indices.data(), Primitive.NumIndices * sizeof(uint32));	Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

	// 색상 constant buffer 생성
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(FVector4);
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = 0;
	URenderer::GetInstance().GetDevice()->CreateBuffer(&cbDesc, nullptr, &ColorBuffer);
}

UBatchLines::~UBatchLines()
{
	SafeRelease(Primitive.InputLayout);
	SafeRelease(Primitive.VertexShader);
	SafeRelease(Primitive.PixelShader);
	SafeRelease(Primitive.VertexBuffer);
	SafeRelease(Primitive.IndexBuffer);
	SafeRelease(ColorBuffer);
}

void UBatchLines::UpdateUGridVertices(const float newCellSize)
{
	if (newCellSize == Grid.GetCellSize()) { return; }
	Grid.UpdateVerticesBy(newCellSize);
	bChangedVertices = true;
}

void UBatchLines::UpdateBoundingBoxVertices(const IBoundingVolume* NewBoundingVolume)
{
	if (NewBoundingVolume && NewBoundingVolume->GetType() != EBoundingVolumeType::SpotLight)
	{
		bRenderSpotLight = false;
	}

	BoundingBoxLines.UpdateVertices(NewBoundingVolume);
	bChangedVertices = true;
}


void UBatchLines::UpdateOctreeVertices(const FOctree* InOctree)
{
	OctreeLines.clear();
	if (InOctree)
	{
		TraverseOctree(InOctree);
	}
	bChangedVertices = true;
}

void UBatchLines::AddShapeComponentVertices(const IBoundingVolume* NewBoundingVolume)
{
	if (!NewBoundingVolume)
	{
		return;
	}

	UBoundingVolumeLines NewLine;
	NewLine.UpdateVertices(NewBoundingVolume);
	ShapeComponentLines.push_back(NewLine);
	bChangedVertices = true;
}

void UBatchLines::AddShapeComponentVertices(const IBoundingVolume* NewBoundingVolume, const FVector4& Color)
{
	if (!NewBoundingVolume)
	{
		return;
	}

	UBoundingVolumeLines NewLine;
	NewLine.UpdateVertices(NewBoundingVolume);
	// ShapeColor는 0-255 범위, 렌더링은 0-1 범위 사용
	NewLine.SetColor(FVector4(Color.X / 255.0f, Color.Y / 255.0f, Color.Z / 255.0f, Color.W / 255.0f));
	ShapeComponentLines.push_back(NewLine);
	bChangedVertices = true;
}

void UBatchLines::UpdateDecalSpotLightVertices(UDecalSpotLightComponent* SpotLightComponent)
{
	if (!SpotLightComponent)
	{
		bRenderSpotLight = false;
		return;
	}

	// GetBoundingBox updates the underlying volume, so we need non-const access.
	const FSpotLightOBB* SpotLightBounding = SpotLightComponent ? SpotLightComponent->GetSpotLightBoundingBox() : nullptr;
	if (!SpotLightBounding)
	{
		bRenderSpotLight = false;
		return;
	}

	SpotLightLines.UpdateVertices(SpotLightBounding);
	bRenderSpotLight = true;
	bChangedVertices = true;
}

void UBatchLines::UpdateConeVertices(const FVector& InCenter, float InGeneratingLineLength
	, float InOuterHalfAngleRad, float InInnerHalfAngleRad, FQuaternion InRotation)
{
	// SpotLight는 scale의 영향을 받지 않으므로 world transformation matrix 직접 계산
	FMatrix TranslationMat = FMatrix::TranslationMatrix(InCenter);
	FMatrix ScaleMat = FMatrix::ScaleMatrix(FVector(InGeneratingLineLength, InGeneratingLineLength, InGeneratingLineLength));
	FMatrix RotationMat = InRotation.ToRotationMatrix();

	if (InGeneratingLineLength <= 0.0f)
	{
		bRenderSpotLight = false;
		return;
	}

	constexpr uint32 NumSegments = 40;
	const float CosOuter = cosf(InOuterHalfAngleRad);
	const float SinOuter = sinf(InOuterHalfAngleRad);
	const float CosInner = InInnerHalfAngleRad > MATH_EPSILON ? cosf(InInnerHalfAngleRad) : 0.0f;
	const float SinInner = InInnerHalfAngleRad > MATH_EPSILON ? sinf(InInnerHalfAngleRad) : 0.0f;

	constexpr float BaseSegmentAngle = (2.0f * PI) / static_cast<float>(NumSegments);
	const float ArcSegmentAngle = 2.0f * InOuterHalfAngleRad / static_cast<float>(NumSegments);
	const bool bHasInnerCone = InInnerHalfAngleRad > MATH_EPSILON;

	TArray<FVector> LocalVertices;
	LocalVertices.reserve(1 + NumSegments + (bHasInnerCone ? NumSegments : 0));
	

	// x, y 평면 위 호 버텍스
	for (uint32 Segment = 0; Segment <= NumSegments; ++Segment)
	{
		const float Angle = -InOuterHalfAngleRad + ArcSegmentAngle * static_cast<float>(Segment);

		LocalVertices.emplace_back(
			cosf(Angle),
			sinf(Angle),
			0.0f
		);
	}

	// z, x 평면 위 호 버텍스
	for (uint32 Segment = 0; Segment <= NumSegments; ++Segment)
	{
		const float Angle = -InOuterHalfAngleRad + ArcSegmentAngle * static_cast<float>(Segment);

		LocalVertices.emplace_back(
			cosf(Angle),
			0.0f,
			sinf(Angle)
		);
	}

	LocalVertices.emplace_back(0.0f, 0.0f, 0.0f); // Apex
	
	// 외곽 원 버텍스
	for (uint32 Segment = 0; Segment < NumSegments; ++Segment)
	{
		const float Angle = BaseSegmentAngle * static_cast<float>(Segment);
		const float CosValue = cosf(Angle);
		const float SinValue = sinf(Angle);

		LocalVertices.emplace_back(
			CosOuter,
			SinOuter * CosValue,
			SinOuter * SinValue
		);
	}
	
	// 내곽 원 버텍스 (있을 경우)
	if (bHasInnerCone)
	{
		for (uint32 Segment = 0; Segment < NumSegments; ++Segment)
		{
			const float Angle = BaseSegmentAngle * static_cast<float>(Segment);
			const float CosValue = cosf(Angle);
			const float SinValue = sinf(Angle);

			LocalVertices.emplace_back(
				CosInner,
				SinInner * CosValue,
				SinInner * SinValue
			);
		}
	}
	
	FMatrix WorldMatrix = ScaleMat;
	WorldMatrix *= RotationMat;
	WorldMatrix *= TranslationMat;

	TArray<FVector> WorldVertices(LocalVertices.size());
	for (size_t Index = 0; Index < LocalVertices.size(); ++Index)
	{
		WorldVertices[Index] = WorldMatrix.TransformPosition(LocalVertices[Index]);
	}

	SpotLightLines.UpdateSpotLightVertices(WorldVertices);
	bRenderSpotLight = true;
	bChangedVertices = true;
}


void UBatchLines::TraverseOctree(const FOctree* InNode)
{
	if (!InNode) { return; }

	UBoundingVolumeLines BoxLines;
	BoxLines.UpdateVertices(&InNode->GetBoundingVolume());
	OctreeLines.push_back(BoxLines);

	if (!InNode->IsLeafNode())
	{
		for (const auto& Child : InNode->GetChildren())
		{
			TraverseOctree(Child);
		}
	}
}

void UBatchLines::UpdateVertexBuffer()
{
	if (bChangedVertices)
	{
		uint32 NumGridVertices = Grid.GetNumVertices();
		uint32 NumBoxVertices = BoundingBoxLines.GetNumVertices();
		uint32 NumShapeComponentVertices = 0;
		for (const auto& Line : ShapeComponentLines)
		{
			NumShapeComponentVertices += Line.GetNumVertices();
		}
		uint32 NumSpotLightVertices = bRenderSpotLight ? SpotLightLines.GetNumVertices() : 0;
		uint32 NumOctreeVertices = 0;
		for (const auto& Line : OctreeLines)
		{
			NumOctreeVertices += Line.GetNumVertices();
		}

		Vertices.resize(NumGridVertices + NumBoxVertices + NumShapeComponentVertices + NumSpotLightVertices + NumOctreeVertices);

		Grid.MergeVerticesAt(Vertices, 0);
		BoundingBoxLines.MergeVerticesAt(Vertices, NumGridVertices);

		uint32 CurrentOffset = NumGridVertices + NumBoxVertices;
		for (auto& Line : ShapeComponentLines)
		{
			Line.MergeVerticesAt(Vertices, CurrentOffset);
			CurrentOffset += Line.GetNumVertices();
		}

		if (bRenderSpotLight)
		{
			SpotLightLines.MergeVerticesAt(Vertices, CurrentOffset);
			CurrentOffset += SpotLightLines.GetNumVertices();
		}

		for (auto& Line : OctreeLines)
		{
			Line.MergeVerticesAt(Vertices, CurrentOffset);
			CurrentOffset += Line.GetNumVertices();
		}

		SetIndices();

		Primitive.NumVertices = static_cast<uint32>(Vertices.size());
		Primitive.NumIndices = static_cast<uint32>(Indices.size());

		SafeRelease(Primitive.VertexBuffer);
		SafeRelease(Primitive.IndexBuffer);

		Primitive.VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(Vertices.data(), Primitive.NumVertices * sizeof(FVector), true);
		Primitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(Indices.data(), Primitive.NumIndices * sizeof(uint32));
	}
	bChangedVertices = false;
}

void UBatchLines::Render()
{
	URenderer& Renderer = URenderer::GetInstance();
	UPipeline* Pipeline = Renderer.GetPipeline();
	ID3D11DeviceContext* Context = Renderer.GetDeviceContext();

	const uint32 NumGridIndices = Grid.GetNumVertices();
	const uint32 NumTotalIndices = Primitive.NumIndices;
	const uint32 NumBoundingVolumeIndices = NumTotalIndices - NumGridIndices;

	// Set up pipeline state (similar to RenderEditorPrimitive)
	FPipelineInfo PipelineInfo = {
		Primitive.InputLayout,
		Primitive.VertexShader,
		FRenderResourceFactory::GetRasterizerState(Primitive.RenderState),
		Primitive.bShouldAlwaysVisible ? Renderer.GetDisabledDepthStencilState() : Renderer.GetDefaultDepthStencilState(),
		Primitive.PixelShader,
		nullptr,
		Primitive.Topology
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// Update model and view/proj constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(Renderer.GetConstantBufferModels(),
		FMatrix::GetModelMatrix(Primitive.Location, Primitive.Rotation, Primitive.Scale));
	Pipeline->SetConstantBuffer(0, EShaderType::VS, Renderer.GetConstantBufferModels());
	Pipeline->SetConstantBuffer(1, EShaderType::VS, Renderer.GetConstantBufferViewProj());

	// Set vertex and index buffers
	Pipeline->SetVertexBuffer(Primitive.VertexBuffer, sizeof(FVector));
	Pipeline->SetIndexBuffer(Primitive.IndexBuffer, sizeof(uint32));

	// 1. Grid 렌더링 (흰색)
	uint32 IndexOffset = 0;
	{
		FVector4 WhiteColor(1.0f, 1.0f, 1.0f, 1.0f);
		Context->UpdateSubresource(ColorBuffer, 0, nullptr, &WhiteColor, 0, 0);
		Context->PSSetConstantBuffers(0, 1, &ColorBuffer);

		Pipeline->DrawIndexed(NumGridIndices, IndexOffset, 0);
		IndexOffset += NumGridIndices;
	}

	// 2. BoundingBoxLines 렌더링 (선택된 컴포넌트 - 설정된 색상 사용)
	const uint32 NumBoundingBoxIndices = BoundingBoxLines.GetNumIndices(BoundingBoxLines.GetCurrentType());
	if (NumBoundingBoxIndices > 0)
	{
		FVector4 BoundingBoxColor = BoundingBoxLines.GetColor();
		Context->UpdateSubresource(ColorBuffer, 0, nullptr, &BoundingBoxColor, 0, 0);
		Context->PSSetConstantBuffers(0, 1, &ColorBuffer);

		Pipeline->DrawIndexed(NumBoundingBoxIndices, IndexOffset, 0);
		IndexOffset += NumBoundingBoxIndices;
	}

	// 3. 각 ShapeComponentLines 렌더링 (각각의 색상)
	for (const auto& ShapeLine : ShapeComponentLines)
	{
		const uint32 NumShapeIndices = ShapeLine.GetNumIndices(ShapeLine.GetCurrentType());
		if (NumShapeIndices > 0)
		{
			FVector4 ShapeColor = ShapeLine.GetColor();
			Context->UpdateSubresource(ColorBuffer, 0, nullptr, &ShapeColor, 0, 0);
			Context->PSSetConstantBuffers(0, 1, &ColorBuffer);

			Pipeline->DrawIndexed(NumShapeIndices, IndexOffset, 0);
			IndexOffset += NumShapeIndices;
		}
	}

	// 4. SpotLightLines 렌더링 (보라색)
	if (bRenderSpotLight)
	{
		const uint32 NumSpotLightIndices = SpotLightLines.GetNumIndices(SpotLightLines.GetCurrentType());
		if (NumSpotLightIndices > 0)
		{
			FVector4 PurpleColor(0.784f, 0.0f, 1.0f, 1.0f);
			Context->UpdateSubresource(ColorBuffer, 0, nullptr, &PurpleColor, 0, 0);
			Context->PSSetConstantBuffers(0, 1, &ColorBuffer);

			Pipeline->DrawIndexed(NumSpotLightIndices, IndexOffset, 0);
			IndexOffset += NumSpotLightIndices;
		}
	}

	// 5. OctreeLines 렌더링 (보라색)
	for (const auto& OctreeLine : OctreeLines)
	{
		const uint32 NumOctreeIndices = OctreeLine.GetNumIndices(OctreeLine.GetCurrentType());
		if (NumOctreeIndices > 0)
		{
			FVector4 PurpleColor(0.784f, 0.0f, 1.0f, 1.0f);
			Context->UpdateSubresource(ColorBuffer, 0, nullptr, &PurpleColor, 0, 0);
			Context->PSSetConstantBuffers(0, 1, &ColorBuffer);

			Pipeline->DrawIndexed(NumOctreeIndices, IndexOffset, 0);
			IndexOffset += NumOctreeIndices;
		}
	}
}

void UBatchLines::SetIndices()
{
	Indices.clear();

	const uint32 NumGridVertices = Grid.GetNumVertices();

	for (uint32 Index = 0; Index < NumGridVertices; ++Index)
	{
		Indices.push_back(Index);
	}

	uint32 BaseVertexOffset = NumGridVertices;

	const EBoundingVolumeType BoundingType = BoundingBoxLines.GetCurrentType();
	int32* BoundingLineIdx = BoundingBoxLines.GetIndices(BoundingType);
	const uint32 NumBoundingIndices = BoundingBoxLines.GetNumIndices(BoundingType);

	if (BoundingLineIdx)
	{
		for (uint32 Idx = 0; Idx < NumBoundingIndices; ++Idx)
		{
			Indices.push_back(BaseVertexOffset + BoundingLineIdx[Idx]);
		}
	}

	BaseVertexOffset += BoundingBoxLines.GetNumVertices();

	for (auto& ShapeLine : ShapeComponentLines)
	{
		const EBoundingVolumeType ShapeComponentType = ShapeLine.GetCurrentType();
		int32* ShapeLineIdx = ShapeLine.GetIndices(ShapeComponentType);
		const uint32 NumShapeComponentIndices = ShapeLine.GetNumIndices(ShapeComponentType);

		if (!ShapeLineIdx)
		{
			continue;
		}

		for (uint32 Idx = 0; Idx < NumShapeComponentIndices; ++Idx)
		{
			Indices.push_back(BaseVertexOffset + ShapeLineIdx[Idx]);
		}

		BaseVertexOffset += ShapeLine.GetNumVertices();
	}

	if (bRenderSpotLight)
	{
		const EBoundingVolumeType SpotLightType = SpotLightLines.GetCurrentType();
		int32* SpotLineIdx = SpotLightLines.GetIndices(SpotLightType);
		const uint32 NumSpotLightIndices = SpotLightLines.GetNumIndices(SpotLightType);

		if (SpotLineIdx)
		{
			for (uint32 Idx = 0; Idx < NumSpotLightIndices; ++Idx)
			{
				Indices.push_back(BaseVertexOffset + SpotLineIdx[Idx]);
			}
		}

		BaseVertexOffset += SpotLightLines.GetNumVertices();
	}

	for (auto& OctreeLine : OctreeLines)
	{
		const EBoundingVolumeType OctreeType = OctreeLine.GetCurrentType();
		int32* OctreeLineIdx = OctreeLine.GetIndices(OctreeType);
		const uint32 NumOctreeIndices = OctreeLine.GetNumIndices(OctreeType);

		if (!OctreeLineIdx)
		{
			continue;
		}

		for (uint32 Idx = 0; Idx < NumOctreeIndices; ++Idx)
		{
			Indices.push_back(BaseVertexOffset + OctreeLineIdx[Idx]);
		}

		BaseVertexOffset += OctreeLine.GetNumVertices();
	}
}

