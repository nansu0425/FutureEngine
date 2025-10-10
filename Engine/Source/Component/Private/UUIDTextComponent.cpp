#include "pch.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UUUIDTextComponent, UTextComponent)

/**
 * @brief Level에서 각 Actor마다 가지고 있는 UUID를 출력해주기 위한 빌보드 클래스
 * Actor has a UBillBoardComponent
 */

UUUIDTextComponent::UUUIDTextComponent() : ZOffset(0.0f) {};

UUUIDTextComponent::~UUUIDTextComponent()
{
}

void UUUIDTextComponent::OnSelected()
{
	SetVisibility(true);
}

void UUUIDTextComponent::OnDeselected()
{
	SetVisibility(false);
}

void UUUIDTextComponent::UpdateRotationMatrix(const FVector& InCameraLocation)
{
	const FVector& OwnerActorLocation = GetOwner()->GetActorLocation();

	FVector ToCamera = InCameraLocation - OwnerActorLocation;
	ToCamera.Normalize();

	const FVector4 worldUp4 = FVector4(0, 0, 1, 1);
	const FVector worldUp = { worldUp4.X, worldUp4.Y, worldUp4.Z };
	FVector Right = worldUp.Cross(ToCamera);
	Right.Normalize();
	FVector Up = ToCamera.Cross(Right);
	Up.Normalize();

	RTMatrix = FMatrix(FVector4(0, 1, 0, 1), worldUp4, FVector4(1,0,0,1));
	RTMatrix = FMatrix(ToCamera, Right, Up);
	//RTMatrix = FMatrix::Identity();
	//UE_LOG("%.2f, %.2f, %.2f", ToCamera.X, ToCamera.Y, ToCamera.Z);

	const FVector Translation = OwnerActorLocation + FVector(0.0f, 0.0f, ZOffset);
	//UE_LOG("%.2f, %.2f, %.2f", Translation.X, Translation.Y, Translation.Z);
	RTMatrix *= FMatrix::TranslationMatrix(Translation);
}

void UUUIDTextComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UTextComponent::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		GetOwner()->SetUUIDTextComponent(this);
		SetOffset(5);
	}
}

UClass* UUUIDTextComponent::GetSpecificWidgetClass() const
{
	return nullptr;
}
