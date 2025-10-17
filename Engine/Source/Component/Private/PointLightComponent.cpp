#include "pch.h"

#include "Component/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "DistanceFalloffExponent", DistanceFalloffExponent);
		SetDistanceFalloffExponent(DistanceFalloffExponent); // clamping을 위해 Setter 사용
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius);
	}
	else
	{
		InOutHandle["DistanceFalloffExponent"] = DistanceFalloffExponent;
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComponent->DistanceFalloffExponent = DistanceFalloffExponent;
	PointLightComponent->AttenuationRadius = AttenuationRadius;

	return PointLightComponent;
}

void UPointLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
    return UPointLightComponentWidget::StaticClass();
}
