#pragma once
#include "Physics/Public/BoundingVolume.h"

struct FOBB : public IBoundingVolume
{
    FOBB() : Center(0.0f, 0.0f, 0.0f), Extents(0.0f, 0.0f, 0.0f), ScaleRotation(FMatrix::Identity())
    {
        
    }
    FOBB(const FVector& InCenter, const FVector& InExtents, const FMatrix& InRotation)
		: Center(InCenter), Extents(InExtents), ScaleRotation(InRotation)
	{}

    struct FAABB ToWorldAABB() const;

	// IBoundingVolume interface
    
    FVector Center;
    FVector Extents;
    FMatrix ScaleRotation;

    
    bool RaycastHit() const override { return false;}
    void Update(const FMatrix& WorldMatrix) override;
    EBoundingVolumeType GetType() const override { return EBoundingVolumeType::OBB; }

    FVector GetExtents()const
    {
        return Extents;
    }
};
