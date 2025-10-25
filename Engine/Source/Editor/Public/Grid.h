#pragma once
#include "Core/Public/Object.h"
#include "Global/CoreTypes.h"
#include "Editor/Public/EditorPrimitive.h"

class UGrid : public UObject
{
public:
	UGrid();
	~UGrid() override;

	void UpdateVerticesBy(float NewCellSize);
	void MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex);

	uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	float GetCellSize() const
	{
		return CellSize;
	}

	void SetCellSize(const float newCellSize)
	{
		CellSize = newCellSize;
	}

private:
	float CellSize = 5.0f;
	int NumLines = 250;
	TArray<FVector> Vertices;
	uint32 NumVertices;
};
