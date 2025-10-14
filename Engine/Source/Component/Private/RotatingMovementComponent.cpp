#include "pch.h"
#include "Component/Public/RotatingMovementComponent.h"

#include "Render/UI/Widget/Public/RotatingMovementComponentWidget.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UMovementComponent)

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
    if (!UpdatedComponent) { return; }

    // Compute new rotation
    const FQuaternion OldRotation = UpdatedComponent->GetWorldRotationAsQuaternion();
    const FQuaternion DeltaRotation = FQuaternion::FromEuler(RotationRate * DeltaTime);
    const FQuaternion NewRotation = bRotationInLocalSpace ? (OldRotation * DeltaRotation) : (DeltaRotation * OldRotation);

    // Compute new location
    FVector DeltaLocation = FVector::ZeroVector();
    if (!PivotTranslation.IsZero())
    {
        const FVector OldPivot = OldRotation.RotateVector(PivotTranslation);
        const FVector NewPivot = NewRotation.RotateVector(PivotTranslation);
        DeltaLocation = (OldPivot - NewPivot);
    }

    MoveUpdatedComponent(DeltaLocation, NewRotation);
}

UClass* URotatingMovementComponent::GetSpecificWidgetClass() const
{
    return URotatingMovementComponentWidget::StaticClass();
}
