#include "pch.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UUUIDTextComponent, UTextComponent)

/**
 * @brief Level에서 각 Actor마다 가지고 있는 UUID를 출력해주기 위한 빌보드 클래스
 * Actor has a UBillBoardComponent
 */

UUUIDTextComponent::UUUIDTextComponent() : ZOffset(5.0f)
{
	SetIsEditorOnly(true);
	SetIsVisualizationComponent(true);
	SetCanPick(false);

	bReceivesDecals = false;
};

UUUIDTextComponent::~UUUIDTextComponent()
{
}

void UUUIDTextComponent::OnSelected()
{
	Super::OnSelected();
}

void UUUIDTextComponent::OnDeselected()
{
	Super::OnDeselected();
}

void UUUIDTextComponent::UpdateRotationMatrix(const FVector& InCameraForward)
{
	FVector Forward = InCameraForward;
	FVector Right = FVector::UpVector().Cross(Forward); Right.Normalize();
	FVector Up = Forward.Cross(Right); Up.Normalize();

	// Construct the rotation matrix from the basis vectors
	RTMatrix = FMatrix(Forward, Right, Up);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Actor의 모든 PrimitiveComponent의 AABB를 계산하여 최상단 위치 결정
	FVector AABBMin = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector AABBMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool bHasPrimitive = false;

	for (UActorComponent* Component : Owner->GetOwnedComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			if (Primitive->IsVisualizationComponent())
			{
				continue;
			}

			FVector CompMin, CompMax;
			Primitive->GetWorldAABB(CompMin, CompMax);

			AABBMin.X = std::min(AABBMin.X, CompMin.X);
			AABBMin.Y = std::min(AABBMin.Y, CompMin.Y);
			AABBMin.Z = std::min(AABBMin.Z, CompMin.Z);

			AABBMax.X = std::max(AABBMax.X, CompMax.X);
			AABBMax.Y = std::max(AABBMax.Y, CompMax.Y);
			AABBMax.Z = std::max(AABBMax.Z, CompMax.Z);

			bHasPrimitive = true;
		}
	}

	FVector Translation;
	if (bHasPrimitive)
	{
		// AABB 박스의 최상단 위치 + ZOffset
		Translation = FVector(Owner->GetActorLocation().X, Owner->GetActorLocation().Y, AABBMax.Z + ZOffset);
	}
	else
	{
		// Primitive가 없는 경우 Actor 위치 + ZOffset
		Translation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, ZOffset);
	}

	RTMatrix *= FMatrix::TranslationMatrix(Translation);
}

void UUUIDTextComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UTextComponent::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		SetOffset(5);
	}
}

UClass* UUUIDTextComponent::GetSpecificWidgetClass() const
{
	return nullptr;
}
