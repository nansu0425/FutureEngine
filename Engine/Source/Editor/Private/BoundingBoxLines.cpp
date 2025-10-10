#include "pch.h"
#include "Editor/Public/BoundingBoxLines.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"

UBoundingBoxLines::UBoundingBoxLines()
	: Vertices(TArray<FVector>())
{
	Vertices.reserve(NumVertices);
	UpdateVertices(GetDisabledBoundingBox());
}

void UBoundingBoxLines::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// 인덱스 범위 보정
	InsertStartIndex = std::min(InsertStartIndex, DestVertices.size());

	// 미리 메모리 확보
	DestVertices.reserve(DestVertices.size() + std::distance(Vertices.begin(), Vertices.end()));

	// 덮어쓸 수 있는 개수 계산
	size_t OverwriteCount = std::min(
		Vertices.size(),
		DestVertices.size() - InsertStartIndex
	);

	// 기존 요소 덮어쓰기
	std::copy(
		Vertices.begin(),
		Vertices.begin() + OverwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}

void UBoundingBoxLines::UpdateVertices(const IBoundingVolume* NewBoundingVolume)
{
	switch (NewBoundingVolume->GetType())
	{
	case EBoundingVolumeType::AABB:
		{
			const FAABB* AABB = static_cast<const FAABB*>(NewBoundingVolume);

			float MinX = AABB->Min.X, MinY = AABB->Min.Y, MinZ = AABB->Min.Z;
			float MaxX = AABB->Max.X, MaxY = AABB->Max.Y, MaxZ = AABB->Max.Z;

			if (Vertices.size() < NumVertices) { Vertices.resize(NumVertices); }

			uint32 Idx = 0;
			Vertices[Idx++] = {MinX, MinY, MinZ}; // Front-Bottom-Left
			Vertices[Idx++] = {MaxX, MinY, MinZ}; // Front-Bottom-Right
			Vertices[Idx++] = {MaxX, MaxY, MinZ}; // Front-Top-Right
			Vertices[Idx++] = {MinX, MaxY, MinZ}; // Front-Top-Left
			Vertices[Idx++] = {MinX, MinY, MaxZ}; // Back-Bottom-Left
			Vertices[Idx++] = {MaxX, MinY, MaxZ}; // Back-Bottom-Right
			Vertices[Idx++] = {MaxX, MaxY, MaxZ}; // Back-Top-Right
			Vertices[Idx] = {MinX, MaxY, MaxZ}; // Back-Top-Left
			break;
		}
	case EBoundingVolumeType::OBB:
		{
			const FOBB* OBB = static_cast<const FOBB*>(NewBoundingVolume);
			const FVector& Extents = OBB->Extents;

			FMatrix OBBToWorld = OBB->ScaleRotation;
			OBBToWorld *= FMatrix::TranslationMatrix(OBB->Center);

			FVector LocalCorners[8] =
			{
				FVector(-Extents.X, -Extents.Y, -Extents.Z), // 0: FBL
				FVector(+Extents.X, -Extents.Y, -Extents.Z), // 1: FBR
				FVector(+Extents.X, +Extents.Y, -Extents.Z), // 2: FTR
				FVector(-Extents.X, +Extents.Y, -Extents.Z), // 3: FTL

				FVector(-Extents.X, -Extents.Y, +Extents.Z), // 4: BBL
				FVector(+Extents.X, -Extents.Y, +Extents.Z), // 5: BBR
				FVector(+Extents.X, +Extents.Y, +Extents.Z), // 6: BTR
				FVector(-Extents.X, +Extents.Y, +Extents.Z)  // 7: BTL
			};

			if (Vertices.size() < NumVertices) { Vertices.resize(NumVertices); }

			for (uint32 Idx = 0; Idx < 8; ++Idx)
			{
				FVector WorldCorner = OBBToWorld.TransformPosition(LocalCorners[Idx]);

				Vertices[Idx] = {WorldCorner.X, WorldCorner.Y, WorldCorner.Z};
			}
			break;
		}
	default:
		break;
	
	}
}

