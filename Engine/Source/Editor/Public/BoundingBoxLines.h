#pragma once
#include "Core/Public/Object.h"
#include "Physics/Public/AABB.h"

class UBoundingBoxLines : UObject
{
public:
	UBoundingBoxLines();
	~UBoundingBoxLines() = default;

	void MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex);
	void UpdateVertices(const IBoundingVolume* NewBoundingVolume);

	uint32 GetNumVertices() const
	{
		return NumVertices;
	}

	FAABB* GetDisabledBoundingBox() { return &DisabledBoundingBox; }

private:
	TArray<FVector> Vertices;
	uint32 NumVertices = 8;
	FAABB DisabledBoundingBox = FAABB(FVector(0, 0, 0), FVector(0, 0, 0));
};

