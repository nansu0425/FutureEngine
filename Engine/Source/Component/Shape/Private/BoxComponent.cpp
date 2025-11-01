#include "pch.h"
#include "Component/Shape/Public/BoxComponent.h"
#include "Component/Shape/Public/SphereComponent.h"
#include "Component/Shape/Public/CapsuleComponent.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/BoundingCapsule.h"
#include "Physics/Public/CollisionDetection.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

UBoxComponent::UBoxComponent()
{
	BoxExtent = FVector(50.0f, 50.0f, 50.0f);
}

UBoxComponent::~UBoxComponent()
{
}

void UBoxComponent::SetBoxExtent(const FVector& InExtent)
{
	BoxExtent = InExtent;
	MarkAsDirty();
}

FAABB UBoxComponent::GetWorldAABB() const
{
	// Local AABB 생성
	FAABB LocalAABB(-BoxExtent, BoxExtent);

	// World Transform 적용
	const FMatrix& WorldTransform = GetWorldTransformMatrix();
	FVector WorldCenter;
	WorldCenter = FVector4(0.f, 0.f, 0.f, 1.f) * WorldTransform;

	// World space의 8개 코너 계산
	FVector LocalCorners[8] =
	{
		FVector(-BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z),
		FVector(+BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z),
		FVector(-BoxExtent.X, +BoxExtent.Y, -BoxExtent.Z),
		FVector(+BoxExtent.X, +BoxExtent.Y, -BoxExtent.Z),
		FVector(-BoxExtent.X, -BoxExtent.Y, +BoxExtent.Z),
		FVector(+BoxExtent.X, -BoxExtent.Y, +BoxExtent.Z),
		FVector(-BoxExtent.X, +BoxExtent.Y, +BoxExtent.Z),
		FVector(+BoxExtent.X, +BoxExtent.Y, +BoxExtent.Z)
	};

	FVector WorldMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
	FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 i = 0; i < 8; i++)
	{
		FVector4 WorldCorner = FVector4(LocalCorners[i].X, LocalCorners[i].Y, LocalCorners[i].Z, 1.0f) * WorldTransform;
		WorldMin.X = min(WorldMin.X, WorldCorner.X);
		WorldMin.Y = min(WorldMin.Y, WorldCorner.Y);
		WorldMin.Z = min(WorldMin.Z, WorldCorner.Z);
		WorldMax.X = max(WorldMax.X, WorldCorner.X);
		WorldMax.Y = max(WorldMax.Y, WorldCorner.Y);
		WorldMax.Z = max(WorldMax.Z, WorldCorner.Z);
	}

	return FAABB(WorldMin, WorldMax);
}

void UBoxComponent::GetWorldAABB(FVector& OutMin, FVector& OutMax)
{
	// 이미 World Space AABB를 계산하므로 직접 반환
	FAABB WorldAABB = GetWorldAABB();
	OutMin = WorldAABB.Min;
	OutMax = WorldAABB.Max;
}

const IBoundingVolume* UBoxComponent::GetBoundingVolume()
{
	// World Space AABB 반환 (BoundingVolumeLines가 World Space 기대, Sphere/Capsule과 일관성 유지)
	CachedWorldAABB = GetWorldAABB();
	return &CachedWorldAABB;
}

bool UBoxComponent::CheckOverlapWith(const UPrimitiveComponent* Other) const
{
	if (!bGenerateOverlapEvents || !Other || !Other->GetGenerateOverlapEvents())
		return false;

	// Box vs Box
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other))
	{
		return CheckOverlapWithBox(OtherBox);
	}

	// Box vs Sphere
	if (const USphereComponent* OtherSphere = Cast<USphereComponent>(Other))
	{
		return CheckOverlapWithSphere(OtherSphere);
	}

	// Box vs Capsule
	if (const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(Other))
	{
		return CheckOverlapWithCapsule(OtherCapsule);
	}

	return false;
}

bool UBoxComponent::CheckOverlapWithBox(const UBoxComponent* OtherBox) const
{
	FAABB MyAABB = GetWorldAABB();
	FAABB OtherAABB = OtherBox->GetWorldAABB();

	// AABB vs AABB 충돌 체크
	return MyAABB.IsIntersected(OtherAABB);
}

bool UBoxComponent::CheckOverlapWithSphere(const USphereComponent* OtherSphere) const
{
	FAABB MyAABB = GetWorldAABB();
	FBoundingSphere OtherSph = OtherSphere->GetWorldSphere();

	return CollisionDetection::CheckOverlap(MyAABB, OtherSph);
}

bool UBoxComponent::CheckOverlapWithCapsule(const UCapsuleComponent* OtherCapsule) const
{
	FAABB MyAABB = GetWorldAABB();
	FBoundingCapsule OtherCap = OtherCapsule->GetWorldCapsule();

	return CollisionDetection::CheckOverlap(MyAABB, OtherCap);
}

void UBoxComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// Box Extent
		if (InOutHandle.hasKey("BoxExtent"))
		{
			JSON& ExtentHandle = InOutHandle["BoxExtent"];
			BoxExtent.X = static_cast<float>(ExtentHandle["X"].ToFloat());
			BoxExtent.Y = static_cast<float>(ExtentHandle["Y"].ToFloat());
			BoxExtent.Z = static_cast<float>(ExtentHandle["Z"].ToFloat());
		}
	}
	else
	{
		// Box Extent
		JSON ExtentHandle = JSON::Make(JSON::Class::Object);
		ExtentHandle["X"] = BoxExtent.X;
		ExtentHandle["Y"] = BoxExtent.Y;
		ExtentHandle["Z"] = BoxExtent.Z;
		InOutHandle["BoxExtent"] = ExtentHandle;
	}
}
