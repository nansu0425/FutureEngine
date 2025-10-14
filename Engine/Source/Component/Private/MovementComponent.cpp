#include "pch.h"
#include "Component/Public/MovementComponent.h"
#include "Component/Public/PrimitiveComponent.h"

IMPLEMENT_ABSTRACT_CLASS(UMovementComponent, UActorComponent)

UMovementComponent::UMovementComponent()
{
    
}

UMovementComponent::~UMovementComponent()
{
    
}

void UMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    AActor* Owner = GetOwner();
    if (Owner)
    {
        SetUpdatedComponent(Owner->GetRootComponent());
    }
}

void UMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
    if (NewUpdatedComponent)
    {
        UpdatedComponent = NewUpdatedComponent;
        UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
        bCanEverTick = true;
    }
    else
    {
        UpdatedComponent = nullptr;
        UpdatedPrimitive = nullptr;
        bCanEverTick = false;
    }
}

void UMovementComponent::MoveUpdatedComponent(const FVector& NewDelta, const FQuaternion& NewRotation)
{
    if (UpdatedComponent)
    {
        UpdatedComponent->SetWorldLocation(UpdatedComponent->GetWorldLocation() + NewDelta);
        UpdatedComponent->SetWorldRotation(NewRotation);
    }
}

void UMovementComponent::StopMovementImmediately()
{
    Velocity = FVector::Zero();
}
