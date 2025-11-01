#include "pch.h"
#include "Physics/Public/CollisionDetection.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/BoundingCapsule.h"

namespace CollisionDetection
{
	bool CheckOverlap(const FAABB& A, const FAABB& B)
	{
		// AABB vs AABB: 각 축에서 겹치는지 확인
		return A.IsIntersected(B);
	}

	bool CheckOverlap(const FBoundingSphere& A, const FBoundingSphere& B)
	{
		// Sphere vs Sphere: 중심간 거리 < 반지름 합
		FVector CenterDiff = A.Center - B.Center;
		float DistanceSquared = CenterDiff.Dot(CenterDiff);
		float RadiusSum = A.Radius + B.Radius;

		return DistanceSquared < (RadiusSum * RadiusSum);
	}

	bool CheckOverlap(const FAABB& Box, const FBoundingSphere& Sphere)
	{
		// AABB vs Sphere: AABB의 가장 가까운 점을 찾고 구와 거리 계산
		FVector ClosestPoint;
		ClosestPoint.X = Clamp(Sphere.Center.X, Box.Min.X, Box.Max.X);
		ClosestPoint.Y = Clamp(Sphere.Center.Y, Box.Min.Y, Box.Max.Y);
		ClosestPoint.Z = Clamp(Sphere.Center.Z, Box.Min.Z, Box.Max.Z);

		FVector Diff = Sphere.Center - ClosestPoint;
		float DistanceSquared = Diff.Dot(Diff);

		return DistanceSquared < (Sphere.Radius * Sphere.Radius);
	}

	bool CheckOverlap(const FBoundingCapsule& A, const FBoundingCapsule& B)
	{
		// Capsule vs Capsule: FBoundingCapsule::Intersects 사용
		return A.Intersects(B);
	}

	bool CheckOverlap(const FBoundingCapsule& Capsule, const FBoundingSphere& Sphere)
	{
		// Capsule vs Sphere: FBoundingCapsule::Intersects 사용
		return Capsule.Intersects(Sphere);
	}

	bool CheckOverlap(const FBoundingCapsule& Capsule, const FAABB& Box)
	{
		// Capsule vs AABB: FBoundingCapsule::Intersects 사용
		return Capsule.Intersects(Box);
	}
}
