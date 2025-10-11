#include "pch.h"
#include "Editor/Public/BatchLines.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Global/Octree.h"

UBatchLines::UBatchLines() : Grid(), BoundingBoxLines()
{
	Vertices.reserve(Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices());
	Vertices.resize(Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices());

	Grid.MergeVerticesAt(Vertices, 0);
	BoundingBoxLines.MergeVerticesAt(Vertices, Grid.GetNumVertices());

	SetIndices();

	URenderer& Renderer = URenderer::GetInstance();

	//BatchLineConstData.CellSize = 1.0f;
	//BatchLineConstData.BoundingBoxModel = FMatrix::Identity();

	/*AddWorldGridVerticesAndConstData();
	AddBoundingBoxVertices();*/

	Primitive.NumVertices = static_cast<uint32>(Vertices.size());
	Primitive.NumIndices = static_cast<uint32>(Indices.size());
	Primitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(Indices.data(), Primitive.NumIndices * sizeof(uint32));
	//Primitive.Color = FVector4(1, 1, 1, 0.2f);
	Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Primitive.Vertexbuffer = FRenderResourceFactory::CreateVertexBuffer(
		Vertices.data(), Primitive.NumVertices * sizeof(FVector), true);
	/*Primitive.Location = FVector(0, 0, 0);
	Primitive.Rotation = FVector(0, 0, 0);
	Primitive.Scale = FVector(1, 1, 1);*/
	Primitive.VertexShader = UAssetManager::GetInstance().GetVertexShader(EShaderType::BatchLine);
	Primitive.InputLayout = UAssetManager::GetInstance().GetIputLayout(EShaderType::BatchLine);
	Primitive.PixelShader = UAssetManager::GetInstance().GetPixelShader(EShaderType::BatchLine);
}

UBatchLines::~UBatchLines()
{
	SafeRelease(Primitive.Vertexbuffer);
	Primitive.InputLayout->Release();
	Primitive.VertexShader->Release();
	Primitive.IndexBuffer->Release();
	Primitive.PixelShader->Release();
}

void UBatchLines::UpdateUGridVertices(const float newCellSize)
{
	if (newCellSize == Grid.GetCellSize())
	{
		return;
	}
	Grid.UpdateVerticesBy(newCellSize);
	bChangedVertices = true;
}

void UBatchLines::UpdateBoundingBoxVertices(const IBoundingVolume* NewBoundingVolume)
{
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

void UBatchLines::TraverseOctree(const FOctree* InNode)
{
	if (!InNode)
	{
		return;
	}

	UBoundingBoxLines BoxLines;
	BoxLines.UpdateVertices(&InNode->GetBoundingBox());
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
		uint32 NumBBoxVertices = BoundingBoxLines.GetNumVertices();
		uint32 NumOctreeVertices = 0;
		for (const auto& Line : OctreeLines)
		{
			NumOctreeVertices += Line.GetNumVertices();
		}

		Vertices.resize(NumGridVertices + NumBBoxVertices + NumOctreeVertices);

		Grid.MergeVerticesAt(Vertices, 0);
		BoundingBoxLines.MergeVerticesAt(Vertices, NumGridVertices);

		uint32 currentOffset = NumGridVertices + NumBBoxVertices;
		for (auto& line : OctreeLines)
		{
			line.MergeVerticesAt(Vertices, currentOffset);
			currentOffset += line.GetNumVertices();
		}

		SetIndices();

		Primitive.NumVertices = static_cast<uint32>(Vertices.size());
		Primitive.NumIndices = static_cast<uint32>(Indices.size());

		SafeRelease(Primitive.Vertexbuffer);
		SafeRelease(Primitive.IndexBuffer);

		Primitive.Vertexbuffer = FRenderResourceFactory::CreateVertexBuffer(Vertices.data(), Primitive.NumVertices * sizeof(FVector), true);
		Primitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(Indices.data(), Primitive.NumIndices * sizeof(uint32));
	}
	bChangedVertices = false;
}

void UBatchLines::Render()
{
	URenderer& Renderer = URenderer::GetInstance();

	// to do: 아래 함수를 batch에 맞게 수정해야 함.
	Renderer.RenderEditorPrimitive(Primitive, Primitive.RenderState, sizeof(FVector), sizeof(uint32));
}

void UBatchLines::SetIndices()
{
	Indices.clear();

	const uint32 numGridVertices = Grid.GetNumVertices();

	// Grid indices
	for (uint32 index = 0; index < numGridVertices; ++index)
	{
		Indices.push_back(index);
	}

	uint32 boundingBoxLineIdx[] = {
		// 앞면
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// 뒷면
		4, 5,
		5, 6,
		6, 7,
		7, 4,

		// 옆면 연결
		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	// BoundingBox indices
	uint32 baseVertexOffset = numGridVertices;
	for (uint32 i = 0; i < std::size(boundingBoxLineIdx); ++i)
	{
		Indices.push_back(baseVertexOffset + boundingBoxLineIdx[i]);
	}

	// OctreeLines indices
	baseVertexOffset += BoundingBoxLines.GetNumVertices();
	for (const auto& octreeLine : OctreeLines)
	{
		for (uint32 i = 0; i < std::size(boundingBoxLineIdx); ++i)
		{
			Indices.push_back(baseVertexOffset + boundingBoxLineIdx[i]);
		}
		baseVertexOffset += octreeLine.GetNumVertices();
	}
}
