#include "pch.h"
#include "Component/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(USpotLightComponent, UPointLightComponent)

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        FJsonSerializer::ReadFloat(InOutHandle, "AngleFalloffExponent", AngleFalloffExponent);
        SetAngleFalloffExponent(AngleFalloffExponent); // clamping을 위해 Setter 사용
        FJsonSerializer::ReadFloat(InOutHandle, "AttenuationAngle", OuterConeAngleRad);
    }
    else
    {
        InOutHandle["AngleFalloffExponent"] = AngleFalloffExponent;
        InOutHandle["AttenuationAngle"] = OuterConeAngleRad;
    }
}

UObject* USpotLightComponent::Duplicate()
{
    USpotLightComponent* NewSpotLightComponent = Cast<USpotLightComponent>(Super::Duplicate());
    NewSpotLightComponent->SetAngleFalloffExponent(AngleFalloffExponent);
    NewSpotLightComponent->SetOuterAngle(OuterConeAngleRad);

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

void USpotLightComponent::SetOuterAngle(float const InAttenuationAngleRad)
{
    OuterConeAngleRad = std::clamp(InAttenuationAngleRad, 0.0f, PI/2.0f - MATH_EPSILON);
    InnerConeAngleRad = std::min(InnerConeAngleRad, OuterConeAngleRad);
}

void USpotLightComponent::SetInnerAngle(float const InAttenuationAngleRad)
{
    InnerConeAngleRad = std::clamp(InAttenuationAngleRad, 0.0f, OuterConeAngleRad);
}