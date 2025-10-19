#include "pch.h"

#include "Component/Public/LightComponent.h"
#include "Utility/Public/JsonSerializer.h"

#include <algorithm>

IMPLEMENT_ABSTRACT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{

}

UObject* ULightComponent::Duplicate()
{
	ULightComponent* LightComponent = Cast<ULightComponent>(Super::Duplicate());

	return LightComponent;
}

void ULightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}


void ULightComponent::SetIntensity(float InIntensity)
{
    Super::SetIntensity(InIntensity);
    UpdateVisualizationBillboardTint();
}

void ULightComponent::SetLightColor(FVector InLightColor)
{
    Super::SetLightColor(InLightColor);
    UpdateVisualizationBillboardTint();
}

void ULightComponent::UpdateVisualizationBillboardTint()
{
    if (!VisualizationBillboard)
    {
        return;
    }

    FVector ClampedColor = GetLightColor();
    ClampedColor.X = std::clamp(ClampedColor.X, 0.0f, 1.0f);
    ClampedColor.Y = std::clamp(ClampedColor.Y, 0.0f, 1.0f);
    ClampedColor.Z = std::clamp(ClampedColor.Z, 0.0f, 1.0f);

    float NormalizedIntensity = std::clamp(GetIntensity(), 0.0f, 20.0f) / 20.0f;

    //FVector4 Tint(ClampedColor.X, ClampedColor.Y, ClampedColor.Z, 1.0f);
    //Tint.W = std::max(NormalizedIntensity, 0.05f);

    FVector4 Tint(ClampedColor.X, ClampedColor.Y, ClampedColor.Z, 0.5f);
    
    
    VisualizationBillboard->SetSpriteTint(Tint);
}
