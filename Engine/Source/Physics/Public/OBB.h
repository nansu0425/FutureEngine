#pragma once
#include "Physics/Public/BoundingVolume.h"

struct FAABB;

struct FOBB : public IBoundingVolume
{
    FOBB(const FVector& InCenter, const FVector& InExtents, const FMatrix& InRotation)
		: Center(InCenter), Extents(InExtents), ScaleRotation(InRotation)
	{}

    struct FAABB ToWorldAABB() const;

	//--- IBoundingVolume interface ---
    
    FVector Center;
    FVector Extents;
    FMatrix ScaleRotation;

    bool RaycastHit() const override { return false;}

    /**  @brief Collision detection algorithm based on SAT */
    bool Intersects(const FAABB& Other);

    /**  @brief Collision detection algorithm based on SAT */
    bool Intersects(const FOBB& Other);

    void Update(const FMatrix& WorldMatrix) override;

    EBoundingVolumeType GetType() const override { return EBoundingVolumeType::OBB; }
};
