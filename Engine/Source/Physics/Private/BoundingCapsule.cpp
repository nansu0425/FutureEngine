#include "pch.h"
#include "Physics/Public/BoundingCapsule.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/BoundingSphere.h"

FBoundingCapsule::FBoundingCapsule()
	: Center(0.f, 0.f, 0.f)
	, HalfHeight(50.f)
	, Radius(25.f)
	, Orientation(FQuaternion::Identity())
{
}

FBoundingCapsule::FBoundingCapsule(const FVector& InCenter, float InHalfHeight, float InRadius)
	: Center(InCenter)
	, HalfHeight(InHalfHeight)
	, Radius(InRadius)
	, Orientation(FQuaternion::Identity())
{
}

FBoundingCapsule::FBoundingCapsule(const FVector& InCenter, float InHalfHeight, float InRadius, const FQuaternion& InOrientation)
	: Center(InCenter)
	, HalfHeight(InHalfHeight)
	, Radius(InRadius)
	, Orientation(InOrientation)
{
}

bool FBoundingCapsule::RaycastHit() const
{
	// TODO: Capsule raycast 구현
	return false;
}

void FBoundingCapsule::Update(const FMatrix& WorldMatrix)
{
	// World Transform 적용
	FVector4 WorldCenter4 = FVector4(Center.X, Center.Y, Center.Z, 1.f) * WorldMatrix;
	Center = FVector(WorldCenter4.X, WorldCenter4.Y, WorldCenter4.Z);

	// Orientation 업데이트 (회전만 추출)
	// 간단하게 처리 - 실제로는 WorldMatrix에서 회전 성분 추출 필요
}

FVector FBoundingCapsule::GetTopSphereCenter() const
{
	// 캡슐의 상단 반구 중심
	// Z축 방향으로 HalfHeight만큼 이동
	FVector Axis = GetAxis();
	return Center + Axis * HalfHeight;
}

FVector FBoundingCapsule::GetBottomSphereCenter() const
{
	// 캡슐의 하단 반구 중심
	// Z축 방향으로 -HalfHeight만큼 이동
	FVector Axis = GetAxis();
	return Center - Axis * HalfHeight;
}

FVector FBoundingCapsule::GetAxis() const
{
	// 캡슐의 중심축 방향 (기본은 Z축, Orientation으로 회전)
	// 간단하게 Z축 방향으로 설정 (향후 Orientation 적용 가능)
	return FVector(0.f, 0.f, 1.f);
}

bool FBoundingCapsule::Intersects(const FBoundingCapsule& Other) const
{
	// Capsule vs Capsule 충돌: 복잡한 알고리즘 필요
	// 간단한 근사: 두 캡슐을 선분 + 반지름으로 보고 선분 간 최단거리 계산

	FVector MyTop = GetTopSphereCenter();
	FVector MyBottom = GetBottomSphereCenter();
	FVector OtherTop = Other.GetTopSphereCenter();
	FVector OtherBottom = Other.GetBottomSphereCenter();

	// 선분 간 최단거리 계산 (간략화: 끝점들 사이의 최소 거리)
	float MinDistSquared = FLT_MAX;

	FVector Points[4][2] = {
		{MyTop, OtherTop}, {MyTop, OtherBottom},
		{MyBottom, OtherTop}, {MyBottom, OtherBottom}
	};

	for (int32 i = 0; i < 4; i++)
	{
		FVector Diff = Points[i][0] - Points[i][1];
		float DistSquared = Diff.Dot(Diff);
		MinDistSquared = min(MinDistSquared, DistSquared);
	}

	float RadiusSum = Radius + Other.Radius;
	return MinDistSquared < (RadiusSum * RadiusSum);
}

bool FBoundingCapsule::Intersects(const FBoundingSphere& Other) const
{
	// Capsule vs Sphere: 구와 캡슐 중심선 사이의 거리 계산
	FVector TopCenter = GetTopSphereCenter();
	FVector BottomCenter = GetBottomSphereCenter();

	// 선분(TopCenter - BottomCenter)과 점(Other.Center) 사이의 최단거리
	FVector LineDir = TopCenter - BottomCenter;
	FVector ToPoint = Other.Center - BottomCenter;

	float LineLengthSquared = LineDir.Dot(LineDir);
	if (LineLengthSquared < 1e-6f)
	{
		// 캡슐이 너무 짧음 - 구와 중심점 간 거리로 계산
		FVector Diff = Other.Center - Center;
		float DistSquared = Diff.Dot(Diff);
		float RadiusSum = Radius + Other.Radius;
		return DistSquared < (RadiusSum * RadiusSum);
	}

	float t = ToPoint.Dot(LineDir) / LineLengthSquared;
	t = Clamp(t, 0.f, 1.f);

	FVector ClosestPoint = BottomCenter + LineDir * t;
	FVector Diff = Other.Center - ClosestPoint;
	float DistSquared = Diff.Dot(Diff);

	float RadiusSum = Radius + Other.Radius;
	return DistSquared < (RadiusSum * RadiusSum);
}

bool FBoundingCapsule::Intersects(const FAABB& Other) const
{
	// Capsule vs AABB: 복잡한 알고리즘
	// 간단한 근사: AABB의 가장 가까운 점을 찾고 캡슐과 거리 계산

	FVector TopCenter = GetTopSphereCenter();
	FVector BottomCenter = GetBottomSphereCenter();

	// AABB의 가장 가까운 점
	FVector ClosestPoint;
	ClosestPoint.X = Clamp(Center.X, Other.Min.X, Other.Max.X);
	ClosestPoint.Y = Clamp(Center.Y, Other.Min.Y, Other.Max.Y);
	ClosestPoint.Z = Clamp(Center.Z, Other.Min.Z, Other.Max.Z);

	// 캡슐 중심선과 ClosestPoint 사이의 거리
	FVector LineDir = TopCenter - BottomCenter;
	FVector ToPoint = ClosestPoint - BottomCenter;

	float LineLengthSquared = LineDir.Dot(LineDir);
	if (LineLengthSquared < 1e-6f)
	{
		FVector Diff = ClosestPoint - Center;
		float DistSquared = Diff.Dot(Diff);
		return DistSquared < (Radius * Radius);
	}

	float t = ToPoint.Dot(LineDir) / LineLengthSquared;
	t = Clamp(t, 0.f, 1.f);

	FVector ClosestOnCapsule = BottomCenter + LineDir * t;
	FVector Diff = ClosestPoint - ClosestOnCapsule;
	float DistSquared = Diff.Dot(Diff);

	return DistSquared < (Radius * Radius);
}
