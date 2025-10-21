#include "pch.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, UWidget)

void UPointLightComponentWidget::Initialize()
{
}

void UPointLightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (PointLightComponent != NewSelectedComponent)
        {
            PointLightComponent = Cast<UPointLightComponent>(NewSelectedComponent);
        }
    }
}

void UPointLightComponentWidget::RenderWidget()
{
    if (!PointLightComponent)
    {
        return;
    }

    ImGui::Separator();
    
    // 모든 입력 필드를 검은색으로 설정
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    // Light Color
    FVector LightColor = PointLightComponent->GetLightColor();
    if (ImGui::ColorEdit3("Light Color", &LightColor.X))
    {
        PointLightComponent->SetLightColor(LightColor);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트 필터 색입니다.\n이 색을 조절하면 실제 라이트의 강도가 조절되는 것과 같은 효과가 생기게 됩니다.");
    }

    // Intensity
    float Intensity = PointLightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        PointLightComponent->SetIntensity(Intensity);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("포인트 라이트 밝기\n범위: 0.0(꺼짐) ~ 20.0(최대)");
    }

    // Attenuation Radius
    float AttenuationRadius = PointLightComponent->GetAttenuationRadius();
    if (ImGui::DragFloat("Attenuation Radius", &AttenuationRadius, 0.1f, 0.0f, 1000.0f))
    {
        PointLightComponent->SetAttenuationRadius(AttenuationRadius);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("빛이 닿는 최대 거리입니다.\n이 반경에서 밝기는 0으로 떨어집니다.");
    }

    // Light Falloff Extent
    float DistanceFalloffExponent = PointLightComponent->GetDistanceFalloffExponent();
    if (ImGui::DragFloat("Distance Falloff Exponent", &DistanceFalloffExponent, 0.1f, 0.0f, 16.0f))
    {
        PointLightComponent->SetDistanceFalloffExponent(DistanceFalloffExponent);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("거리에 따라 밝기가 줄어드는 속도를 조절합니다.\n값이 클수록 감소가 더 급격합니다.");
    }
    
    ImGui::PopStyleColor(3);

    ImGui::Separator();
}

