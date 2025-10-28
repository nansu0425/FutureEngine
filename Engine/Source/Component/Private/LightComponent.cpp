#include "pch.h"

#include "Component/Public/LightComponent.h"
#include "Utility/Public/JsonSerializer.h"

#include "Component/Public/EditorIconComponent.h"
#include "Actor/Public/Actor.h"

#include <algorithm>

IMPLEMENT_ABSTRACT_CLASS(ULightComponent, ULightComponentBase)

void ULightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        float LoadedShadowResolutionScale = ShadowResolutionScale;
        float LoadedShadowBias = ShadowBias;
        float LoadedShadowSlopeBias = ShadowSlopeBias;
        float LoadedShadowSharpen = ShadowSharpen;

        FJsonSerializer::ReadFloat(InOutHandle, "ShadowResolutionScale", LoadedShadowResolutionScale);
        FJsonSerializer::ReadFloat(InOutHandle, "ShadowBias", LoadedShadowBias);
        FJsonSerializer::ReadFloat(InOutHandle, "ShadowSlopeBias", LoadedShadowSlopeBias);
        FJsonSerializer::ReadFloat(InOutHandle, "ShadowSharpen", LoadedShadowSharpen);

        SetShadowResolutionScale(LoadedShadowResolutionScale);
        SetShadowBias(LoadedShadowBias);
        SetShadowSlopeBias(LoadedShadowSlopeBias);
        SetShadowSharpen(LoadedShadowSharpen);
    }
    else
    {
        InOutHandle["ShadowResolutionScale"] = ShadowResolutionScale;
        InOutHandle["ShadowBias"] = ShadowBias;
        InOutHandle["ShadowSlopeBias"] = ShadowSlopeBias;
        InOutHandle["ShadowSharpen"] = ShadowSharpen;
    }
}

UObject* ULightComponent::Duplicate()
{
	ULightComponent* LightComponent = Cast<ULightComponent>(Super::Duplicate());

	LightComponent->ShadowResolutionScale = ShadowResolutionScale;
	LightComponent->ShadowBias = ShadowBias;
	LightComponent->ShadowSlopeBias = ShadowSlopeBias;
	LightComponent->ShadowSharpen = ShadowSharpen;

	return LightComponent;
}

void ULightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}


void ULightComponent::SetIntensity(float InIntensity)
{
    Super::SetIntensity(InIntensity);
    UpdateVisualizationIconTint();
}

void ULightComponent::SetLightColor(FVector InLightColor)
{
    Super::SetLightColor(InLightColor);
    UpdateVisualizationIconTint();
}

void ULightComponent::UpdateVisualizationIconTint()
{
    if (!VisualizationIcon)
    {
        return;
    }

    FVector ClampedColor = GetLightColor();
    ClampedColor.X = std::clamp(ClampedColor.X, 0.0f, 1.0f);
    ClampedColor.Y = std::clamp(ClampedColor.Y, 0.0f, 1.0f);
    ClampedColor.Z = std::clamp(ClampedColor.Z, 0.0f, 1.0f);

    float NormalizedIntensity = std::clamp(GetIntensity(), 0.0f, 20.0f) / 20.0f;
    FVector4 Tint(ClampedColor.X, ClampedColor.Y, ClampedColor.Z, 1.0f);
    VisualizationIcon->SetSpriteTint(Tint);
}

void ULightComponent::RefreshVisualizationIconBinding()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    UEditorIconComponent* BoundIcon = VisualizationIcon;
    const bool bNeedsLookup = !BoundIcon || !BoundIcon->IsVisualizationComponent() || BoundIcon->GetAttachParent() != this;

    if (bNeedsLookup)
    {
        BoundIcon = nullptr;
        for (UActorComponent* Component : OwnerActor->GetOwnedComponents())
        {
            if (UEditorIconComponent* Candidate = Cast<UEditorIconComponent>(Component))
            {
                if (!Candidate->IsVisualizationComponent())
                {
                    continue;
                }

                if (Candidate->GetAttachParent() != this)
                {
                    continue;
                }

                BoundIcon = Candidate;
                break;
            }
        }

        VisualizationIcon = BoundIcon;
    }

    if (VisualizationIcon)
    {
        if (VisualizationIcon->GetAttachParent() != this)
        {
            VisualizationIcon->AttachToComponent(this);
        }

        UpdateVisualizationIconTint();
    }
}
