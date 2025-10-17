#include "pch.h"
#include "Component/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(USpotLightComponent, ULightComponent)

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "DistanceFalloffExponent", DistanceFalloffExponent, 2.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "AngleFalloffExponent", AngleFalloffExponent, 64.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius, 10.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "AttenuationAngle", AttenuationAngleRad, PI / 4.0f);
    }
    else
    {
        InOutHandle["DistanceFalloffExponent"] = DistanceFalloffExponent;
        InOutHandle["AngleFalloffExponent"] = AngleFalloffExponent;
        InOutHandle["AttenuationRadius"] = AttenuationRadius;
        InOutHandle["AttenuationAngle"] = AttenuationAngleRad;
    }
}

UObject* USpotLightComponent::Duplicate()
{
    USpotLightComponent* NewSpotLightComponent = Cast<USpotLightComponent>(Super::Duplicate());
    NewSpotLightComponent->DistanceFalloffExponent = DistanceFalloffExponent;
    NewSpotLightComponent->AngleFalloffExponent = AngleFalloffExponent;
    NewSpotLightComponent->AttenuationRadius = AttenuationRadius;
    NewSpotLightComponent->AttenuationAngleRad = AttenuationAngleRad;
    
    return NewSpotLightComponent;
}

void USpotLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
    Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* USpotLightComponent::GetSpecificWidgetClass() const
{
    return USpotLightComponentWidget::StaticClass();
}
