#pragma once
#include "Physics/Public/BoundingVolume.h"

struct FOBB : public IBoundingVolume
{
    FOBB(const FVector& InCenter, const FVector& InExtents, const FMatrix& InRotation)
		: Center(InCenter), Extents(InExtents), ScaleRotation(InRotation)
	{}

    struct FAABB ToWorldAABB() const;

	// IBoundingVolume interface
    
    FVector Center;
    FVector Extents;
    FMatrix ScaleRotation;

    // 정규직교 회전행렬
    FMatrix Rotation;

    
    bool RaycastHit() const override { return false;}
    void Update(const FMatrix& WorldMatrix) override;
    EBoundingVolumeType GetType() const override { return EBoundingVolumeType::OBB; }

    FVector GetExtents()const
    {
        return Extents;
    }
};
