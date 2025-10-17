#include "pch.h"
#include "Render/UI/Widget/Public/AmbientLightComponentWidget.h"
#include "Component/Public/AmbientLightComponent.h"

IMPLEMENT_CLASS(UAmbientLightComponentWidget, UWidget)

void UAmbientLightComponentWidget::Update()
{
	ULevel* CurrentLevel = GWorld->GetLevel();
	if (CurrentLevel)
	{
		UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
		if (AmbientLightComponent != NewSelectedComponent)
		{
			AmbientLightComponent = Cast<UAmbientLightComponent>(NewSelectedComponent);
		}
	}
}

void UAmbientLightComponentWidget::RenderWidget()
{
	if (!AmbientLightComponent)
	{
		return;
	}
	
	FVector LightColor = AmbientLightComponent->GetLightColor();
	if (ImGui::ColorEdit3("Light Color", &LightColor.X))
	{
		AmbientLightComponent->SetLightColor(LightColor);
	}

	float Intensity = AmbientLightComponent->GetIntensity();
	if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
	{
		AmbientLightComponent->SetIntensity(Intensity);
	}

	ImGui::Separator();

}
