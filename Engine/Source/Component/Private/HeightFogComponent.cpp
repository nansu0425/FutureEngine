#include "pch.h"
#include "Component/Public/HeightFogComponent.h"
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
        // FJsonSerializer::ReadVector(InOutHandle, "Location", RelativeLocation, FVector::ZeroVector());
        // FVector RotationEuler;
        // FJsonSerializer::ReadVector(InOutHandle, "Rotation", RotationEuler, FVector::ZeroVector());
        // RelativeRotation = FQuaternion::FromEuler(RotationEuler);
        //
        // FJsonSerializer::ReadVector(InOutHandle, "Scale", RelativeScale3D, FVector::OneVector());
    }
    // 저장
    else
    {
        // InOutHandle["Location"] = FJsonSerializer::VectorToJson(RelativeLocation);
        // InOutHandle["Rotation"] = FJsonSerializer::VectorToJson(RelativeRotation.ToEuler());
        // InOutHandle["Scale"] = FJsonSerializer::VectorToJson(RelativeScale3D);
    }
}

UClass* UHeightFogComponent::GetSpecificWidgetClass() const
{
    return Super::GetSpecificWidgetClass();
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
