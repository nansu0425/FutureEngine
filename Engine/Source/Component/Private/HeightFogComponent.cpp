#include "pch.h"
#include "Component/Public/HeightFogComponent.h"

#include "Render/UI/Widget/Public/HeightFogComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UHeightFogComponent, USceneComponent)

UHeightFogComponent::UHeightFogComponent()
{
    bCanEverTick = false;
}

UHeightFogComponent::~UHeightFogComponent()
{
    
}

void UHeightFogComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}

void UHeightFogComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    // 불러오기
    if (bInIsLoading)
    {
        std::string tempString;
        FJsonSerializer::ReadVector(InOutHandle, "FogInScatteringColor", FogInScatteringColor, {0.5f, 0.5f, 0.5f});

        FJsonSerializer::ReadString(InOutHandle, "FogDensity", tempString, "0.05");
        FogDensity = std::stof(tempString);
        FJsonSerializer::ReadString(InOutHandle, "FogHeightFalloff", tempString, "0.01");
        FogHeightFalloff = std::stof(tempString);
        FJsonSerializer::ReadString(InOutHandle, "StartDistance", tempString, "1.5");
        StartDistance = std::stof(tempString);
        FJsonSerializer::ReadString(InOutHandle, "FogCutoffDistance", tempString, "50000.0");
        FogCutoffDistance = std::stof(tempString);
        FJsonSerializer::ReadString(InOutHandle, "FogMaxOpacity", tempString, "0.98");
        FogMaxOpacity = std::stof(tempString);
    }
    // 저장
    else
    {
        InOutHandle["FogInScatteringColor"] = FJsonSerializer::VectorToJson(FogInScatteringColor);
        InOutHandle["FogDensity"] = to_string(FogDensity);
        InOutHandle["FogHeightFalloff"] = to_string(FogHeightFalloff);
        InOutHandle["StartDistance"] = to_string(StartDistance);
        InOutHandle["FogCutoffDistance"] = to_string(FogCutoffDistance);
        InOutHandle["FogMaxOpacity"] = to_string(FogMaxOpacity);
    }
}

UClass* UHeightFogComponent::GetSpecificWidgetClass() const
{
	return UHeightFogComponentWidget::StaticClass();
}

UObject* UHeightFogComponent::Duplicate()
{
    UHeightFogComponent* HeightFogComponent = Cast<UHeightFogComponent>(Super::Duplicate());
    
    HeightFogComponent->FogDensity = FogDensity;
    HeightFogComponent->FogHeightFalloff = FogHeightFalloff;
    HeightFogComponent->StartDistance = StartDistance;
    HeightFogComponent->FogCutoffDistance = FogCutoffDistance;
    HeightFogComponent->FogMaxOpacity = FogMaxOpacity;

    return HeightFogComponent;
}
