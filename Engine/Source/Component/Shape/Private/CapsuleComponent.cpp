#include "pch.h"
#include "Component/Shape/Public/CapsuleComponent.h"
#include "Component/Shape/Public/BoxComponent.h"
#include "Component/Shape/Public/SphereComponent.h"
#include "Physics/Public/BoundingCapsule.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/BoundingSphere.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

UCapsuleComponent::UCapsuleComponent()
{
}

UCapsuleComponent::~UCapsuleComponent()
{
}

void UCapsuleComponent::SetCapsuleHalfHeight(float InHalfHeight)
{
	CapsuleHalfHeight = InHalfHeight;
	MarkAsDirty();
}

void UCapsuleComponent::SetCapsuleRadius(float InRadius)
{
	CapsuleRadius = InRadius;
	MarkAsDirty();
}

FBoundingCapsule UCapsuleComponent::GetWorldCapsule() const
{
	// World Transform에서 위치 추출
	const FMatrix& WorldTransform = GetWorldTransformMatrix();
	FVector WorldCenter;
	WorldCenter = FVector4(0.f, 0.f, 0.f, 1.f) * WorldTransform;

	// Scale 적용
	FVector WorldScale = GetWorldScale3D();
	float ScaleXY = max(WorldScale.X, WorldScale.Y);
	float WorldRadius = CapsuleRadius * ScaleXY;
	float WorldHalfHeight = CapsuleHalfHeight * WorldScale.Z;

	// Rotation 추출
	FQuaternion WorldOrientation = GetWorldRotationAsQuaternion();

	return FBoundingCapsule(WorldCenter, WorldHalfHeight, WorldRadius, WorldOrientation);
}

const IBoundingVolume* UCapsuleComponent::GetBoundingVolume()
{
	CachedWorldCapsule = GetWorldCapsule();
	return &CachedWorldCapsule;
}

bool UCapsuleComponent::CheckOverlapWith(const UPrimitiveComponent* Other) const
{
	if (!bGenerateOverlapEvents || !Other || !Other->GetGenerateOverlapEvents())
		return false;

	// Capsule vs Box
	if (const UBoxComponent* OtherBox = Cast<UBoxComponent>(Other))
	{
		return CheckOverlapWithBox(OtherBox);
	}

	// Capsule vs Sphere
	if (const USphereComponent* OtherSphere = Cast<USphereComponent>(Other))
	{
		return CheckOverlapWithSphere(OtherSphere);
	}

	// Capsule vs Capsule
	if (const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(Other))
	{
		return CheckOverlapWithCapsule(OtherCapsule);
	}

	return false;
}

bool UCapsuleComponent::CheckOverlapWithBox(const UBoxComponent* OtherBox) const
{
	FBoundingCapsule MyCapsule = GetWorldCapsule();
	FAABB OtherAABB = OtherBox->GetWorldAABB();

	return MyCapsule.Intersects(OtherAABB);
}

bool UCapsuleComponent::CheckOverlapWithSphere(const USphereComponent* OtherSphere) const
{
	FBoundingCapsule MyCapsule = GetWorldCapsule();
	FBoundingSphere OtherSph = OtherSphere->GetWorldSphere();

	return MyCapsule.Intersects(OtherSph);
}

bool UCapsuleComponent::CheckOverlapWithCapsule(const UCapsuleComponent* OtherCapsule) const
{
	FBoundingCapsule MyCapsule = GetWorldCapsule();
	FBoundingCapsule OtherCap = OtherCapsule->GetWorldCapsule();

	return MyCapsule.Intersects(OtherCap);
}

UObject* UCapsuleComponent::Duplicate()
{
	UCapsuleComponent* DuplicatedCapsuleComponent = Cast<UCapsuleComponent>(Super::Duplicate());

	// CapsuleComponent 고유 속성 복사
	DuplicatedCapsuleComponent->CapsuleHalfHeight = CapsuleHalfHeight;
	DuplicatedCapsuleComponent->CapsuleRadius = CapsuleRadius;

	return DuplicatedCapsuleComponent;
}

void UCapsuleComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// Capsule Half Height
		if (InOutHandle.hasKey("CapsuleHalfHeight"))
		{
			CapsuleHalfHeight = static_cast<float>(InOutHandle["CapsuleHalfHeight"].ToFloat());
		}

		// Capsule Radius
		if (InOutHandle.hasKey("CapsuleRadius"))
		{
			CapsuleRadius = static_cast<float>(InOutHandle["CapsuleRadius"].ToFloat());
		}
	}
	else
	{
		// Capsule Half Height
		InOutHandle["CapsuleHalfHeight"] = CapsuleHalfHeight;

		// Capsule Radius
		InOutHandle["CapsuleRadius"] = CapsuleRadius;
	}
}
