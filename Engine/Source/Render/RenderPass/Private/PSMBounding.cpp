#include "Render/RenderPass/Public/PSMBounding.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include <cmath>
#include <algorithm>

#define ALMOST_ZERO(F) (std::abs(F) < 1e-6f)
#define IS_SPECIAL(F) (std::isnan(F) || std::isinf(F))

// Helper function: Normalize plane (a, b, c, d)
static FVector4 NormalizePlane(const FVector4& Plane)
{
	float Length = std::sqrt(Plane.X * Plane.X + Plane.Y * Plane.Y + Plane.Z * Plane.Z);
	if (ALMOST_ZERO(Length))
		return Plane;
	float InvLength = 1.0f / Length;
	return FVector4(Plane.X * InvLength, Plane.Y * InvLength, Plane.Z * InvLength, Plane.W * InvLength);
}

// Helper: Plane-point signed distance
static float PlaneDistance(const FVector4& Plane, const FVector& Point)
{
	return Plane.X * Point.X + Plane.Y * Point.Y + Plane.Z * Point.Z + Plane.W;
}

// Helper: Three-plane intersection
static bool PlaneIntersection(FVector& OutPoint, const FVector4& P0, const FVector4& P1, const FVector4& P2)
{
	FVector N0(P0.X, P0.Y, P0.Z);
	FVector N1(P1.X, P1.Y, P1.Z);
	FVector N2(P2.X, P2.Y, P2.Z);

	FVector N1xN2 = N1.Cross(N2);
	FVector N2xN0 = N2.Cross(N0);
	FVector N0xN1 = N0.Cross(N1);

	float CosTheta = N0.Dot(N1xN2);

	if (ALMOST_ZERO(CosTheta) || IS_SPECIAL(CosTheta))
		return false;

	float SecTheta = 1.0f / CosTheta;

	N1xN2 = N1xN2 * P0.W;
	N2xN0 = N2xN0 * P1.W;
	N0xN1 = N0xN1 * P2.W;

	OutPoint = -(N1xN2 + N2xN0 + N0xN1) * SecTheta;
	return true;
}

//-----------------------------------------------------------------------------
// FPSMBoundingBox
//-----------------------------------------------------------------------------

bool FPSMBoundingBox::Intersect(float& OutHitDist, const FVector& Origin, const FVector& Direction) const
{
	// Slab method for ray-AABB intersection
	FVector4 Planes[6] = {
		FVector4(1, 0, 0, -MinPt.X),  FVector4(-1, 0, 0, MaxPt.X),
		FVector4(0, 1, 0, -MinPt.Y),  FVector4(0, -1, 0, MaxPt.Y),
		FVector4(0, 0, 1, -MinPt.Z),  FVector4(0, 0, -1, MaxPt.Z)
	};

	OutHitDist = 0.0f;
	FVector HitPoint = Origin;
	bool Inside = false;

	for (int i = 0; i < 6 && !Inside; i++)
	{
		FVector PlaneNormal(Planes[i].X, Planes[i].Y, Planes[i].Z);
		float CosTheta = PlaneNormal.Dot(Direction);
		float Dist = PlaneDistance(Planes[i], Origin);

		if (ALMOST_ZERO(Dist))
			return true;
		if (ALMOST_ZERO(CosTheta))
			continue;

		OutHitDist = -Dist / CosTheta;
		if (OutHitDist < 0.0f)
			continue;

		HitPoint = Origin + Direction * OutHitDist;
		Inside = true;

		// Check if hit point is inside AABB on other dimensions
		for (int j = 0; j < 6 && Inside; j++)
		{
			if (j == i)
				continue;
			float D = PlaneDistance(Planes[j], HitPoint);
			Inside = (D + 0.00015f) >= 0.0f;
		}
	}

	return Inside;
}

//-----------------------------------------------------------------------------
// FPSMBoundingSphere
//-----------------------------------------------------------------------------

FPSMBoundingSphere::FPSMBoundingSphere(const std::vector<FVector>& Points)
{
	if (Points.empty())
	{
		Center = FVector::ZeroVector();
		Radius = 0.0f;
		return;
	}

	// Ritter's bounding sphere algorithm (approximate)
	Center = Points[0];
	Radius = 0.0f;

	for (size_t i = 1; i < Points.size(); i++)
	{
		FVector CenterToPoint = Points[i] - Center;
		float DistSq = CenterToPoint.LengthSquared();

		if (DistSq > Radius * Radius)
		{
			float Dist = std::sqrt(DistSq);
			float NewRadius = 0.5f * (Dist + Radius);
			float Scale = (NewRadius - Radius) / Dist;
			Center = Center + CenterToPoint * Scale;
			Radius = NewRadius;
		}
	}
}

//-----------------------------------------------------------------------------
// FPSMFrustum
//-----------------------------------------------------------------------------

FPSMFrustum::FPSMFrustum(const FMatrix& ViewProj)
{
	// Extract frustum planes from view-projection matrix (DirectX row-major)
	FVector4 Col0(ViewProj.Data[0][0], ViewProj.Data[1][0], ViewProj.Data[2][0], ViewProj.Data[3][0]);
	FVector4 Col1(ViewProj.Data[0][1], ViewProj.Data[1][1], ViewProj.Data[2][1], ViewProj.Data[3][1]);
	FVector4 Col2(ViewProj.Data[0][2], ViewProj.Data[1][2], ViewProj.Data[2][2], ViewProj.Data[3][2]);
	FVector4 Col3(ViewProj.Data[0][3], ViewProj.Data[1][3], ViewProj.Data[2][3], ViewProj.Data[3][3]);

	Planes[0] = NormalizePlane(Col3 + Col0);  // Left
	Planes[1] = NormalizePlane(Col3 - Col0);  // Right
	Planes[2] = NormalizePlane(Col3 + Col1);  // Bottom
	Planes[3] = NormalizePlane(Col3 - Col1);  // Top
	Planes[4] = NormalizePlane(Col3 + Col2);  // Near
	Planes[5] = NormalizePlane(Col3 - Col2);  // Far

	// Build vertex LUT for AABB test
	for (int i = 0; i < 6; i++)
	{
		VertexLUT[i] = ((Planes[i].X < 0.0f) ? 1 : 0) |
			((Planes[i].Y < 0.0f) ? 2 : 0) |
			((Planes[i].Z < 0.0f) ? 4 : 0);
	}

	// Compute frustum corners
	for (int i = 0; i < 8; i++)
	{
		const FVector4& P0 = (i & 1) ? Planes[4] : Planes[5];  // Near/Far
		const FVector4& P1 = (i & 2) ? Planes[3] : Planes[2];  // Top/Bottom
		const FVector4& P2 = (i & 4) ? Planes[0] : Planes[1];  // Left/Right

		PlaneIntersection(Corners[i], P0, P1, P2);
	}
}

bool FPSMFrustum::TestSphere(const FPSMBoundingSphere& Sphere) const
{
	for (int i = 0; i < 6; i++)
	{
		float Dist = PlaneDistance(Planes[i], Sphere.Center);
		if (Dist + Sphere.Radius < 0.0f)
			return false;
	}
	return true;
}

int FPSMFrustum::TestBox(const FPSMBoundingBox& Box) const
{
	bool Intersect = false;

	for (int i = 0; i < 6; i++)
	{
		int NV = VertexLUT[i];
		FVector NVertex(
			(NV & 1) ? Box.MinPt.X : Box.MaxPt.X,
			(NV & 2) ? Box.MinPt.Y : Box.MaxPt.Y,
			(NV & 4) ? Box.MinPt.Z : Box.MaxPt.Z
		);
		FVector PVertex(
			(NV & 1) ? Box.MaxPt.X : Box.MinPt.X,
			(NV & 2) ? Box.MaxPt.Y : Box.MinPt.Y,
			(NV & 4) ? Box.MaxPt.Z : Box.MinPt.Z
		);

		if (PlaneDistance(Planes[i], NVertex) < 0.0f)
			return 0;  // Outside
		if (PlaneDistance(Planes[i], PVertex) < 0.0f)
			Intersect = true;
	}

	return Intersect ? 2 : 1;  // 2 = Intersecting, 1 = Fully inside
}

bool SweptSpherePlaneIntersect(
	float& OutT0,
	float& OutT1,
	const FVector4& Plane,
	const FPSMBoundingSphere& Sphere,
	const FVector& SweepDir)
{
	FVector PlaneNormal(Plane.X, Plane.Y, Plane.Z);
	float BDotN = PlaneDistance(Plane, Sphere.Center);
	float DDotN = SweepDir.Dot(PlaneNormal);

	if (ALMOST_ZERO(DDotN))
	{
		if (BDotN <= Sphere.Radius)
		{
			OutT0 = 0.0f;
			OutT1 = 1e32f;
			return true;
		}
		return false;
	}

	float Tmp0 = (Sphere.Radius - BDotN) / DDotN;
	float Tmp1 = (-Sphere.Radius - BDotN) / DDotN;
	OutT0 = std::min(Tmp0, Tmp1);
	OutT1 = std::max(Tmp0, Tmp1);
	return true;
}

bool FPSMFrustum::TestSweptSphere(const FPSMBoundingSphere& Sphere, const FVector& SweepDir) const
{
	std::vector<float> Displacements;
	Displacements.reserve(12);

	for (int i = 0; i < 6; i++)
	{
		float T0, T1;
		if (SweptSpherePlaneIntersect(T0, T1, Planes[i], Sphere, SweepDir))
		{
			if (T0 >= 0.0f)
				Displacements.push_back(T0);
			if (T1 >= 0.0f)
				Displacements.push_back(T1);
		}
	}

	for (float T : Displacements)
	{
		FPSMBoundingSphere DisplacedSphere = Sphere;
		DisplacedSphere.Center = Sphere.Center + SweepDir * T;
		DisplacedSphere.Radius *= 1.1f;  // Small tolerance
		if (TestSphere(DisplacedSphere))
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// FPSMBoundingCone
//-----------------------------------------------------------------------------

FPSMBoundingCone::FPSMBoundingCone(
	const std::vector<FPSMBoundingBox>& Boxes,
	const FMatrix& Projection,
	const FVector& InApex)
	: Apex(InApex)
{
	if (Boxes.empty())
	{
		Direction = FVector(1, 0, 0);  // X-Forward
		FovX = FovY = 0.0f;
		LookAtMatrix = FMatrix::Identity();
		return;
	}

	// Transform all box corners to post-projective space
	std::vector<FVector> PPPoints;
	PPPoints.reserve(Boxes.size() * 8);

	for (const auto& Box : Boxes)
	{
		for (int i = 0; i < 8; i++)
		{
			FVector Corner = Box.GetCorner(i);
			FVector4 Transformed = Projection.TransformVector4(FVector4(Corner, 1.0f));
			if (std::abs(Transformed.W) > 1e-6f)
			{
				PPPoints.push_back(FVector(
					Transformed.X / Transformed.W,
					Transformed.Y / Transformed.W,
					Transformed.Z / Transformed.W
				));
			}
		}
	}

	if (PPPoints.empty())
	{
		Direction = FVector(1, 0, 0);
		FovX = FovY = 0.0f;
		LookAtMatrix = FMatrix::Identity();
		return;
	}

	// Compute bounding sphere to find optimal direction
	FPSMBoundingSphere BSphere(PPPoints);
	Direction = (BSphere.Center - Apex).GetNormalized();

	// Choose up vector (Z-Up in FutureEngine, but in view space this might differ)
	FVector Up(0, 0, 1);
	if (std::abs(Direction.Z) > 0.99f)
		Up = FVector(1, 0, 0);  // Use X-Forward if direction is nearly vertical

	// Create LookAt matrix
	FVector Target = Apex + Direction;
	LookAtMatrix = FMatrix::CreateLookAtLH(Apex, Target, Up);

	// Transform points to light space and compute FOV
	Near = FLT_MAX;
	Far = -FLT_MAX;
	float MaxX = 0.0f, MaxY = 0.0f;

	for (const auto& Pt : PPPoints)
	{
		FVector4 LSPoint = LookAtMatrix.TransformVector4(FVector4(Pt, 1.0f));
		if (LSPoint.Z > 1e-6f)  // Z is depth in light space
		{
			MaxX = std::max(MaxX, std::abs(LSPoint.X / LSPoint.Z));
			MaxY = std::max(MaxY, std::abs(LSPoint.Y / LSPoint.Z));
			Near = std::min(Near, LSPoint.Z);
			Far = std::max(Far, LSPoint.Z);
		}
	}

	FovX = std::atan(MaxX);
	FovY = std::atan(MaxY);
}

FPSMBoundingCone::FPSMBoundingCone(
	const std::vector<FPSMBoundingBox>& Boxes,
	const FMatrix& Projection,
	const FVector& InApex,
	const FVector& InDirection)
	: Apex(InApex), Direction(InDirection.GetNormalized())
{
	// Choose up vector
	FVector Up(0, 0, 1);
	if (std::abs(Direction.Z) > 0.99f)
		Up = FVector(1, 0, 0);

	FVector Target = Apex + Direction;
	LookAtMatrix = FMatrix::CreateLookAtLH(Apex, Target, Up);

	// Transform all box corners
	std::vector<FVector> LSPoints;
	LSPoints.reserve(Boxes.size() * 8);

	for (const auto& Box : Boxes)
	{
		for (int i = 0; i < 8; i++)
		{
			FVector Corner = Box.GetCorner(i);
			FVector4 PP = Projection.TransformVector4(FVector4(Corner, 1.0f));
			if (std::abs(PP.W) > 1e-6f)
			{
				FVector PPCorner(PP.X / PP.W, PP.Y / PP.W, PP.Z / PP.W);
				LSPoints.push_back(PPCorner);
			}
		}
	}

	Near = FLT_MAX;
	Far = -FLT_MAX;
	float MaxX = 0.0f, MaxY = 0.0f;

	for (const auto& Pt : LSPoints)
	{
		FVector4 LSPoint = LookAtMatrix.TransformVector4(FVector4(Pt, 1.0f));
		if (LSPoint.Z > 1e-6f)
		{
			MaxX = std::max(MaxX, std::abs(LSPoint.X / LSPoint.Z));
			MaxY = std::max(MaxY, std::abs(LSPoint.Y / LSPoint.Z));
			Near = std::min(Near, LSPoint.Z);
			Far = std::max(Far, LSPoint.Z);
		}
	}

	FovX = std::atan(MaxX);
	FovY = std::atan(MaxY);
}

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

void TransformBoundingBox(FPSMBoundingBox& Result, const FPSMBoundingBox& Source, const FMatrix& Transform)
{
	Result.MinPt = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	Result.MaxPt = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int i = 0; i < 8; i++)
	{
		FVector Corner = Source.GetCorner(i);
		FVector4 Transformed = Transform.TransformVector4(FVector4(Corner, 1.0f));
		FVector TransformedCorner(Transformed.X, Transformed.Y, Transformed.Z);
		Result.Merge(TransformedCorner);
	}
}

void GetMeshWorldBoundingBox(FPSMBoundingBox& OutBox, UStaticMeshComponent* Mesh)
{
	if (!Mesh)
		return;

	FVector WorldMin, WorldMax;
	Mesh->GetWorldAABB(WorldMin, WorldMax);

	OutBox.MinPt = WorldMin;
	OutBox.MaxPt = WorldMax;
}
