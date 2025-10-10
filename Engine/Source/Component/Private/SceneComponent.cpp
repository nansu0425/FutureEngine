#include "pch.h"
#include "Component/Public/SceneComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Utility/Public/JsonSerializer.h"

#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"

#include <json.hpp>

IMPLEMENT_CLASS(USceneComponent, UActorComponent)

USceneComponent::USceneComponent()
{
	ComponentType = EComponentType::Scene;
}

void USceneComponent::BeginPlay()
{

}

void USceneComponent::TickComponent()
{
	Super::TickComponent();
}

void USceneComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		FJsonSerializer::ReadVector(InOutHandle, "Location", RelativeLocation, FVector::ZeroVector());
		FJsonSerializer::ReadVector(InOutHandle, "Rotation", RelativeRotation, FVector::ZeroVector());
		FJsonSerializer::ReadVector(InOutHandle, "Scale", RelativeScale3D, FVector::OneVector());
		MarkAsDirty();
	}
	// 저장
	else
	{
		InOutHandle["Location"] = FJsonSerializer::VectorToJson(RelativeLocation);
		InOutHandle["Rotation"] = FJsonSerializer::VectorToJson(RelativeRotation);
		InOutHandle["Scale"] = FJsonSerializer::VectorToJson(RelativeScale3D);
	}
}

void USceneComponent::SetParentAttachment(USceneComponent* NewParent)
{
	if (NewParent == this || NewParent == ParentAttachment)
	{
		return;
	}
	for (USceneComponent* Ancester = NewParent; Ancester != nullptr; Ancester = Ancester->ParentAttachment)
	{
		// 만약 거슬러 올라가다 나 자신을 만나면 순환 구조이므로 함수를 종료합니다.
		if (Ancester == this)
		{
			return; 
		}
	}

	// 기존 부모가 있었다면, 그 부모의 자식 목록에서 나를 제거합니다.
	if (ParentAttachment)
	{
		ParentAttachment->RemoveChild(this);
	}

	// 새로운 부모를 설정합니다.
	ParentAttachment = NewParent;

	// 새로운 부모가 있다면, 그 부모의 자식 목록에 나를 추가합니다.
	if (ParentAttachment)
	{
		ParentAttachment->Children.push_back(this);
	}

	MarkAsDirty();
}
void USceneComponent::RemoveChild(USceneComponent* ChildDeleted)
{
	Children.erase(std::remove(Children.begin(), Children.end(), ChildDeleted), Children.end());
}

UObject* USceneComponent::Duplicate()
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(Super::Duplicate());
	SceneComponent->RelativeLocation = RelativeLocation;
	SceneComponent->RelativeRotation = RelativeRotation;
	SceneComponent->RelativeScale3D = RelativeScale3D;
	SceneComponent->MarkAsDirty();
	return SceneComponent;
}

void USceneComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
	USceneComponent* SceneComponent = Cast<USceneComponent>(DuplicatedObject);
	for (USceneComponent* Child : Children)
	{
		USceneComponent* ReplicatedChild = Cast<USceneComponent>(Child->Duplicate());
		ReplicatedChild->SetParentAttachment(SceneComponent);
	}
}

void USceneComponent::MarkAsDirty()
{
	bIsTransformDirty = true;
	bIsTransformDirtyInverse = true;

	for (USceneComponent* Child : Children)
	{
		Child->MarkAsDirty();
	}
}

void USceneComponent::SetRelativeLocation(const FVector& Location)
{
	RelativeLocation = Location;
	MarkAsDirty();

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(this))
	{
		GWorld->GetLevel()->UpdatePrimitiveInOctree(PrimitiveComponent);
	}
}

void USceneComponent::SetRelativeRotation(const FVector& Rotation)
{
	RelativeRotation = Rotation;
	MarkAsDirty();

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(this))
	{
		GWorld->GetLevel()->UpdatePrimitiveInOctree(PrimitiveComponent);
	}
}

void USceneComponent::SetRelativeScale3D(const FVector& Scale)
{
	RelativeScale3D = Scale;
	MarkAsDirty();

	if (auto PrimitiveComponent = Cast<UPrimitiveComponent>(this))
	{
		GWorld->GetLevel()->UpdatePrimitiveInOctree(PrimitiveComponent);
	}
}

const FMatrix& USceneComponent::GetWorldTransformMatrix() const
{
	if (bIsTransformDirty)
	{
		WorldTransformMatrix = FMatrix::GetModelMatrix(RelativeLocation, FVector::GetDegreeToRadian(RelativeRotation), RelativeScale3D);

		if (ParentAttachment)
		{
			WorldTransformMatrix *= ParentAttachment->GetWorldTransformMatrix();
		}

		bIsTransformDirty = false;
	}

	return WorldTransformMatrix;
}

const FMatrix& USceneComponent::GetWorldTransformMatrixInverse() const
{
	if (bIsTransformDirtyInverse)
	{
		WorldTransformMatrixInverse = FMatrix::Identity();

		if (ParentAttachment)
		{
			WorldTransformMatrixInverse *= ParentAttachment->GetWorldTransformMatrixInverse();
		}

		WorldTransformMatrixInverse *= FMatrix::GetModelMatrixInverse(RelativeLocation, FVector::GetDegreeToRadian(RelativeRotation), RelativeScale3D);

		bIsTransformDirtyInverse = false;
	}

	return WorldTransformMatrixInverse;
}

FVector USceneComponent::GetWorldLocation() const
{
    return GetWorldTransformMatrix().GetLocation();
}

FVector USceneComponent::GetWorldRotation() const
{
    return GetWorldTransformMatrix().GetRotation();
}

FVector USceneComponent::GetWorldScale3D() const
{
    return GetWorldTransformMatrix().GetScale();
}

void USceneComponent::SetWorldLocation(const FVector& NewLocation)
{
    if (ParentAttachment)
    {
        const FMatrix ParentWorldMatrixInverse = ParentAttachment->GetWorldTransformMatrixInverse();
        SetRelativeLocation(ParentWorldMatrixInverse.TransformPosition(NewLocation));
    }
    else
    {
        SetRelativeLocation(NewLocation);
    }
}

void USceneComponent::SetWorldRotation(const FVector& NewRotation)
{
    if (ParentAttachment)
    {
        const FVector ParentWorldRotation = ParentAttachment->GetWorldRotation();
        SetRelativeRotation(NewRotation - ParentWorldRotation);
    }
    else
    {
        SetRelativeRotation(NewRotation);
    }
}

void USceneComponent::SetWorldScale3D(const FVector& NewScale)
{
    if (ParentAttachment)
    {
        const FVector ParentWorldScale = ParentAttachment->GetWorldScale3D();
        SetRelativeScale3D(FVector(NewScale.X / ParentWorldScale.X, NewScale.Y / ParentWorldScale.Y, NewScale.Z / ParentWorldScale.Z));
    }
    else
    {
        SetRelativeScale3D(NewScale);
    }
}