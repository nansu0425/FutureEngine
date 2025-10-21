#include "pch.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"
#include "Component/Public/SpotLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(USpotLightComponentWidget, UWidget)

void USpotLightComponentWidget::Initialize()
{
}

void USpotLightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (SpotLightComponent != NewSelectedComponent)
        {
            SpotLightComponent = Cast<USpotLightComponent>(NewSelectedComponent);
        }
    }
}

void USpotLightComponentWidget::RenderWidget()
{
    if (!SpotLightComponent)
    {
        return;
    }

    ImGui::Separator();
    
    // 모든 입력 필드를 검은색으로 설정
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    // Light Color
    FVector LightColor = SpotLightComponent->GetLightColor();
    if (ImGui::ColorEdit3("Light Color", &LightColor.X))
    {
        SpotLightComponent->SetLightColor(LightColor);
    }

    // Intensity
    float Intensity = SpotLightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        SpotLightComponent->SetIntensity(Intensity);
    }

    // Distance Falloff Exponent
    float DistanceFalloffExponent = SpotLightComponent->GetDistanceFalloffExponent();
    if (ImGui::DragFloat("Distance Falloff Exponent", &DistanceFalloffExponent, 0.1f, 2.0f, 16.0f))
    {
        SpotLightComponent->SetDistanceFalloffExponent(DistanceFalloffExponent);
    }

    // Angle Falloff Exponent
    float AngleFalloffExponent = SpotLightComponent->GetAngleFalloffExponent();
    if (ImGui::DragFloat("Angle Falloff Exponent", &AngleFalloffExponent, 0.5f, 1.0f, 128.0f))
    {
        SpotLightComponent->SetAngleFalloffExponent(AngleFalloffExponent);
    }

    // Attenuation Radius
    float AttenuationRadius = SpotLightComponent->GetAttenuationRadius();
    if (ImGui::DragFloat("Attenuation Radius", &AttenuationRadius, 0.1f, 0.0f, 1000.0f))
    {
        SpotLightComponent->SetAttenuationRadius(AttenuationRadius);
    }

    // Outer Cone Angle
    float OuterAngleDegrees = SpotLightComponent->GetOuterConeAngle() * ToDeg;
    if (ImGui::DragFloat("Outer Cone Angle (deg)", &OuterAngleDegrees, 1.0f, 0.0f, 89.0f))
    {
        SpotLightComponent->SetOuterAngle(OuterAngleDegrees * ToRad);
    }

    // Inner Cone Angle
    float InnerAngleDegrees = SpotLightComponent->GetInnerConeAngle() * ToDeg;
    if (ImGui::DragFloat("Inner Cone Angle (deg)", &InnerAngleDegrees, 1.0f, 0.0f, OuterAngleDegrees))
    {
        SpotLightComponent->SetInnerAngle(InnerAngleDegrees * ToRad);
    }
    
    ImGui::PopStyleColor(3);

    ImGui::Separator();
}
