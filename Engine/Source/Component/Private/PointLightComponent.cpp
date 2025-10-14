#include "pch.h"

#include "Component/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponent, USceneComponent)

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
    return UPointLightComponentWidget::StaticClass();
}
