#include "pch.h"
#include "Component/Public/AmbientLightComponent.h"
#include "Render/UI/Widget/Public/AmbientLightComponentWidget.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent)

void UAmbientLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{

}

UObject* UAmbientLightComponent::Duplicate()
{
	UAmbientLightComponent* AmbientLightComponent = Cast<UAmbientLightComponent>(Super::Duplicate());

	return AmbientLightComponent;
}

void  UAmbientLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UAmbientLightComponent::GetSpecificWidgetClass() const
{
	return UAmbientLightComponentWidget::StaticClass();
}
