#include "pch.h"
#include "Render/RenderPass/Public/EditorIconPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"

FEditorIconPass::FEditorIconPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
	ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS), BS(InBS)
{
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
	EditorIconMaterialConstants.MaterialFlags |= HAS_DIFFUSE_MAP;
	EditorIconMaterialConstants.Kd = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
}

void FEditorIconPass::Execute(FRenderingContext& Context)
{
	FRenderState RenderState = UEditorIconComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None;
		RenderState.FillMode = EFillMode::WireFrame;
	}
	FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState(RenderState), DS, PS, BS, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
	Pipeline->UpdatePipeline(PipelineInfo);

	// EditorIcon은 Billboard 플래그와 무관하게 항상 렌더링
	// PIE 모드에서는 렌더링 X (Context에 아예 추가되지 않음)

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, EditorIconMaterialConstants);
	Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferMaterial);

	// EditorIcon Sort
	struct FDistanceSortedEditorIcon
	{
		UEditorIconComponent* EditorIcon;
		float DistanceSq;
	};

	std::vector<FDistanceSortedEditorIcon> SortedEditorIcons;
	FVector CameraLocation = Context.CurrentCamera->GetLocation();

	for (UEditorIconComponent* EditorIconComp : Context.EditorIcons)
	{
		EditorIconComp->FaceCamera(Context.CurrentCamera->GetForward());
		FVector EditorIconLocation = EditorIconComp->GetWorldLocation();
		float DistanceSq = FVector::DistSquared(CameraLocation, EditorIconLocation);
		SortedEditorIcons.push_back({ EditorIconComp, DistanceSq });
	}

	// DistanceSq가 클수록 앞에 오도록 정렬 (먼 것부터 렌더링)
	std::sort(SortedEditorIcons.begin(), SortedEditorIcons.end(), [](const FDistanceSortedEditorIcon& a, const FDistanceSortedEditorIcon& b)
	{
		return a.DistanceSq > b.DistanceSq;
	});

	for (const auto& SortedItem : SortedEditorIcons)
	{
		UEditorIconComponent* EditorIconComp = SortedItem.EditorIcon;
		if (!EditorIconComp->IsVisible())
		{
			continue;
		}
		const FVector4 Tint = EditorIconComp->GetSpriteTint();
		EditorIconMaterialConstants.Ka = Tint;
		EditorIconMaterialConstants.Kd = Tint;
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, EditorIconMaterialConstants);
		Pipeline->SetConstantBuffer(2, EShaderType::PS, ConstantBufferMaterial);

		FMatrix WorldMatrix;
		if (EditorIconComp->IsScreenSizeScaled())
		{
			FVector FixedWorldScale = EditorIconComp->GetRelativeScale3D();
			FVector EditorIconLocation = EditorIconComp->GetWorldLocation();
			FQuaternion EditorIconRotation = EditorIconComp->GetWorldRotationAsQuaternion();

			WorldMatrix = FMatrix::GetModelMatrix(EditorIconLocation, EditorIconRotation, FixedWorldScale);
		}
		else
		{
			WorldMatrix = EditorIconComp->GetWorldTransformMatrix();
		}

		Pipeline->SetVertexBuffer(EditorIconComp->GetVertexBuffer(), sizeof(FNormalVertex));
		Pipeline->SetIndexBuffer(EditorIconComp->GetIndexBuffer(), 0);

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
		Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);

		Pipeline->SetShaderResourceView(0, EShaderType::PS, EditorIconComp->GetSprite()->GetTextureSRV());
		Pipeline->SetSamplerState(0, EShaderType::PS, EditorIconComp->GetSprite()->GetTextureSampler());

		Pipeline->DrawIndexed(EditorIconComp->GetNumIndices(), 0, 0);
	}
}

void FEditorIconPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
}
