#pragma once

#include "Global/Vector.h"
#include "Global/Matrix.h"
#include "Global/Types.h"
#include <vector>
#include <algorithm>

class UStaticMeshComponent;

/**
 * @brief Axis-Aligned Bounding Box
 */
struct FPSMBoundingBox
{
	FVector MinPt = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxPt = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	FPSMBoundingBox() = default;
	FPSMBoundingBox(const FPSMBoundingBox& Other) : MinPt(Other.MinPt), MaxPt(Other.MaxPt) {}

	/**
	 * @brief Construct AABB from array of points
	 */
	explicit FPSMBoundingBox(const std::vector<FVector>& Points)
	{
		for (const auto& Pt : Points)
		{
			Merge(Pt);
		}
	}

	/**
	 * @brief Merge multiple AABBs into one
	 */
	explicit FPSMBoundingBox(const std::vector<FPSMBoundingBox>& Boxes)
	{
		for (const auto& Box : Boxes)
		{
			Merge(Box.MinPt);
			Merge(Box.MaxPt);
		}
	}

	/**
	 * @brief Get center of AABB
	 */
	FVector GetCenter() const
	{
		return (MinPt + MaxPt) * 0.5f;
	}

	/**
	 * @brief Merge a point into this AABB
	 */
	void Merge(const FVector& Point)
	{
		MinPt.X = std::min(MinPt.X, Point.X);
		MinPt.Y = std::min(MinPt.Y, Point.Y);
		MinPt.Z = std::min(MinPt.Z, Point.Z);

		MaxPt.X = std::max(MaxPt.X, Point.X);
		MaxPt.Y = std::max(MaxPt.Y, Point.Y);
		MaxPt.Z = std::max(MaxPt.Z, Point.Z);
	}

	/**
	 * @brief Get i-th corner of AABB (0-7)
	 */
	FVector GetCorner(int Index) const
	{
		return FVector(
			(Index & 1) ? MaxPt.X : MinPt.X,
			(Index & 2) ? MaxPt.Y : MinPt.Y,
			(Index & 4) ? MaxPt.Z : MinPt.Z
		);
	}

	/**
	 * @brief Ray-box intersection test
	 * @param OutHitDist Distance along ray to intersection point
	 * @param Origin Ray origin
	 * @param Direction Ray direction (should be normalized)
	 * @return true if intersection exists
	 */
	bool Intersect(float& OutHitDist, const FVector& Origin, const FVector& Direction) const;

	/**
	 * @brief Check if AABB is valid (non-empty)
	 */
	bool IsValid() const
	{
		return MinPt.X <= MaxPt.X && MinPt.Y <= MaxPt.Y && MinPt.Z <= MaxPt.Z;
	}
};

/**
 * @brief Bounding Sphere
 */
struct FPSMBoundingSphere
{
	FVector Center = FVector::ZeroVector();
	float Radius = 0.0f;

	FPSMBoundingSphere() = default;
	FPSMBoundingSphere(const FPSMBoundingSphere& Other) : Center(Other.Center), Radius(Other.Radius) {}

	/**
	 * @brief Construct bounding sphere from AABB
	 */
	explicit FPSMBoundingSphere(const FPSMBoundingBox& Box)
	{
		Center = Box.GetCenter();
		FVector RadiusVec = Box.MaxPt - Center;
		Radius = RadiusVec.Length();
	}

	/**
	 * @brief Construct minimum bounding sphere from points
	 */
	explicit FPSMBoundingSphere(const std::vector<FVector>& Points);
};

/**
 * @brief View Frustum for culling tests
 */
struct FPSMFrustum
{
	FVector4 Planes[6];  // Left, Right, Bottom, Top, Near, Far (in DirectX view space)
	FVector Corners[8];  // 8 frustum corners
	int VertexLUT[6];    // Nearest/farthest vertex index for each plane

	FPSMFrustum() = default;

	/**
	 * @brief Construct frustum from view-projection matrix
	 * @param ViewProj Combined view-projection matrix
	 */
	explicit FPSMFrustum(const FMatrix& ViewProj);

	/**
	 * @brief Test if sphere is inside frustum
	 */
	bool TestSphere(const FPSMBoundingSphere& Sphere) const;

	/**
	 * @brief Test if AABB is inside/intersecting frustum
	 * @return 0 = outside, 1 = fully inside, 2 = intersecting
	 */
	int TestBox(const FPSMBoundingBox& Box) const;

	/**
	 * @brief Test if swept sphere (extruded along direction) intersects frustum
	 * Used for shadow caster culling
	 */
	bool TestSweptSphere(const FPSMBoundingSphere& Sphere, const FVector& SweepDir) const;
};

/**
 * @brief Bounding Cone - used for tight FOV calculation in PSM
 */
struct FPSMBoundingCone
{
	FVector Apex = FVector::ZeroVector();
	FVector Direction = FVector(1, 0, 0);  // X-Forward in FutureEngine
	float FovX = 0.0f;  // Half angle in radians
	float FovY = 0.0f;  // Half angle in radians
	float Near = 0.001f;
	float Far = 1.0f;
	FMatrix LookAtMatrix = FMatrix::Identity();

	FPSMBoundingCone() = default;

	/**
	 * @brief Construct bounding cone from AABBs and apex point
	 * Automatically calculates optimal view direction and FOV
	 *
	 * @param Boxes AABBs in post-projective space
	 * @param Projection Transformation to post-projective space
	 * @param InApex Cone apex (light position in post-projective space)
	 */
	FPSMBoundingCone(
		const std::vector<FPSMBoundingBox>& Boxes,
		const FMatrix& Projection,
		const FVector& InApex
	);

	/**
	 * @brief Construct bounding cone with predefined direction
	 *
	 * @param Boxes AABBs in post-projective space
	 * @param Projection Transformation to post-projective space
	 * @param InApex Cone apex
	 * @param InDirection Cone direction (will be normalized)
	 */
	FPSMBoundingCone(
		const std::vector<FPSMBoundingBox>& Boxes,
		const FMatrix& Projection,
		const FVector& InApex,
		const FVector& InDirection
	);
};

/**
 * @brief Transform AABB by matrix
 * @param Result Output transformed AABB
 * @param Source Input AABB
 * @param Transform Transformation matrix
 */
void TransformBoundingBox(FPSMBoundingBox& Result, const FPSMBoundingBox& Source, const FMatrix& Transform);

/**
 * @brief Get AABB of a static mesh component in world space
 */
void GetMeshWorldBoundingBox(FPSMBoundingBox& OutBox, UStaticMeshComponent* Mesh);

/**
 * @brief Test swept sphere-plane intersection (for shadow caster culling)
 */
bool SweptSpherePlaneIntersect(
	float& OutT0,
	float& OutT1,
	const FVector4& Plane,
	const FPSMBoundingSphere& Sphere,
	const FVector& SweepDir
);
