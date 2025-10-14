#pragma once
#include "MovementComponent.h"

class URotatingMovementComponent : public UMovementComponent
{
	DECLARE_CLASS(URotatingMovementComponent, UMovementComponent);
	
public:
	virtual void TickComponent(float DeltaTime) override;
	
	FVector RotationRate;
	FVector PivotTranslation;
	bool bRotationInLocalSpace;

public:
	UClass* GetSpecificWidgetClass() const override;

};
