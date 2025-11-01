#pragma once
#include "Physics/Public/BoundingVolume.h"
#include "Global/Vector.h"

struct FBoundingSphere : public IBoundingVolume
{
	FVector Center;
	float Radius;

	FBoundingSphere() : Center(0.f, 0.f, 0.f), Radius(0.f) {}
	FBoundingSphere(const FVector& InCenter, float InRadius) : Center(InCenter), Radius(InRadius) {}

	bool RaycastHit() const override;
	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::Sphere; }
};
