#include "pch.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UUUIDTextComponent, UTextComponent)

/**
 * @brief Level에서 각 Actor마다 가지고 있는 UUID를 출력해주기 위한 빌보드 클래스
 * Actor has a UBillBoardComponent
 */

UUUIDTextComponent::UUUIDTextComponent()
{
	SetIsEditorOnly(true);
	SetCanPick(false);
};

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

void UUUIDTextComponent::FaceCamera(const FVector& InCameraForward)
{	
	FVector Forward = InCameraForward;
	FVector Right = Forward.Cross(FVector::UpVector()); Right.Normalize();
	FVector Up = Right.Cross(Forward); Up.Normalize();
    
	// Construct the rotation matrix from the basis vectors
	FMatrix RotationMatrix = FMatrix(Forward, Right, Up);
    
	// Convert the rotation matrix to a quaternion and set the relative rotation
	SetWorldRotation(FQuaternion::FromRotationMatrix(RotationMatrix));
}

void UUUIDTextComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UTextComponent::Serialize(bInIsLoading, InOutHandle);
}

UClass* UUUIDTextComponent::GetSpecificWidgetClass() const
{
	return nullptr;
}
