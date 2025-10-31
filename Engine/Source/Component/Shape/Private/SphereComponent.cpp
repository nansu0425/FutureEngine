#include "pch.h"
#include "Component/Shape/Public/SphereComponent.h"
#include "Component/Shape/Public/BoxComponent.h"
#include "Component/Shape/Public/CapsuleComponent.h"
#include "Physics/Public/BoundingSphere.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/BoundingCapsule.h"
#include "Physics/Public/CollisionDetection.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(USphereComponent, UShapeComponent)

USphereComponent::USphereComponent()
{
	SphereRadius = 50.0f;
}

USphereComponent::~USphereComponent()
{
}

void USphereComponent::SetSphereRadius(float InRadius)
{
	SphereRadius = InRadius;
	MarkAsDirty();
}

FBoundingSphere USphereComponent::GetWorldSphere() const
{
	// World Transform에서 위치 추출
	const FMatrix& WorldTransform = GetWorldTransformMatrix();
	FVector WorldCenter;
	WorldCenter = FVector4(0.f, 0.f, 0.f, 1.f) * WorldTransform;

	// Scale 적용 (균일 스케일 가정)
	FVector WorldScale = GetWorldScale3D();
	float MaxScale = max(max(WorldScale.X, WorldScale.Y), WorldScale.Z);
	float WorldRadius = SphereRadius * MaxScale;

	return FBoundingSphere(WorldCenter, WorldRadius);
}

bool USphereComponent::CheckOverlapWith(const UPrimitiveComponent* Other) const
{
	if (!bGenerateOverlapEvents || !Other || !Other->GetGenerateOverlapEvents())
		return false;

	// Sphere vs Box
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other))
	{
		return CheckOverlapWithBox(OtherBox);
	}

	// Sphere vs Sphere
	if (const USphereComponent* OtherSphere = Cast<USphereComponent>(Other))
	{
		return CheckOverlapWithSphere(OtherSphere);
	}

	// Sphere vs Capsule
	if (const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(Other))
	{
		return CheckOverlapWithCapsule(OtherCapsule);
	}

	return false;
}

bool USphereComponent::CheckOverlapWithBox(const UBoxComponent* OtherBox) const
{
	FBoundingSphere MySphere = GetWorldSphere();
	FAABB OtherAABB = OtherBox->GetWorldAABB();

	return CollisionDetection::CheckOverlap(MySphere, OtherAABB);
}

bool USphereComponent::CheckOverlapWithSphere(const USphereComponent* OtherSphere) const
{
	FBoundingSphere MySphere = GetWorldSphere();
	FBoundingSphere OtherSph = OtherSphere->GetWorldSphere();

	// Sphere vs Sphere: 중심간 거리 < 반지름 합
	FVector CenterDiff = MySphere.Center - OtherSph.Center;
	float DistanceSquared = CenterDiff.Dot(CenterDiff);
	float RadiusSum = MySphere.Radius + OtherSph.Radius;

	return DistanceSquared < (RadiusSum * RadiusSum);
}

bool USphereComponent::CheckOverlapWithCapsule(const UCapsuleComponent* OtherCapsule) const
{
	FBoundingSphere MySphere = GetWorldSphere();
	FBoundingCapsule OtherCap = OtherCapsule->GetWorldCapsule();

	return CollisionDetection::CheckOverlap(MySphere, OtherCap);
}

void USphereComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// Sphere Radius
		if (InOutHandle.hasKey("SphereRadius"))
		{
			SphereRadius = static_cast<float>(InOutHandle["SphereRadius"].ToFloat());
		}
	}
	else
	{
		// Sphere Radius
		InOutHandle["SphereRadius"] = SphereRadius;
	}
}
