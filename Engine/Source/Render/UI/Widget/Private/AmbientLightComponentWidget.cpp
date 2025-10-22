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
	
	// 모든 입력 필드를 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	// 라이트 활성화 체크박스
	bool LightEnabled = AmbientLightComponent->GetLightEnabled();
	if (ImGui::Checkbox("Light Enabled", &LightEnabled))
	{
		AmbientLightComponent->SetLightEnabled(LightEnabled);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("라이트를 켜고 끕니다.\n끄면 조명 계산에서 제외됩니다.\n(Outliner O/X와는 별개)");
	}
	FVector LightColor = AmbientLightComponent->GetLightColor();
	if (ImGui::ColorEdit3("Light Color", &LightColor.X))
	{
		AmbientLightComponent->SetLightColor(LightColor);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("환경광의 색상입니다.\n씬 전체에 균일한 기본 조명을 제공합니다.");
	}

	float Intensity = AmbientLightComponent->GetIntensity();
	if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
	{
		AmbientLightComponent->SetIntensity(Intensity);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("환경광 밝기\n범위: 0.0(꺼짐) ~ 20.0(최대)");
	}
	
	ImGui::PopStyleColor(3);

	ImGui::Separator();

}
