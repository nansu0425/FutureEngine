#include "pch.h"

#include "Component/Public/LightComponentBase.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(ULightComponentBase, USceneComponent)

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        float LoadedIntensity = Intensity;
        FVector LoadedColor = LightColor;
        FJsonSerializer::ReadFloat(InOutHandle, "Intensity", LoadedIntensity);
        FJsonSerializer::ReadVector(InOutHandle, "LightColor", LoadedColor);

        SetIntensity(LoadedIntensity);
        SetLightColor(LoadedColor);
    }
    else
    {
        InOutHandle["Intensity"] = Intensity;
        InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
    }
}

UObject* ULightComponentBase::Duplicate()
{
    ULightComponentBase* LightComponentBase = Cast<ULightComponentBase>(Super::Duplicate());
    LightComponentBase->Intensity = Intensity;
    LightComponentBase->LightColor = LightColor;
    LightComponentBase->bVisible = bVisible;
    return LightComponentBase;
}

void ULightComponentBase::DuplicateSubObjects(UObject* DuplicatedObject)
{
    Super::DuplicateSubObjects(DuplicatedObject);
}
