#include "pch.h"
#include "Render/UI/Widget/Public/HeightFogComponentWidget.h"
#include "Component/Public/HeightFogComponent.h"
#include "Component/Public/ActorComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "ImGui/imgui.h"

IMPLEMENT_CLASS(UHeightFogComponentWidget, UWidget)

void UHeightFogComponentWidget::Initialize()
{
}

void UHeightFogComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (FogComponent != NewSelectedComponent)
        {
            FogComponent = Cast<UHeightFogComponent>(NewSelectedComponent);
        }
    }
}

void UHeightFogComponentWidget::RenderWidget()
{
    if (!FogComponent)
    {
        return;
    }

    ImGui::Separator();
    ImGui::PushItemWidth(150.0f);
    
    // 입력 필드 배경색을 검은색으로 설정
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    // FogDensity
    float density = FogComponent->GetFogDensity();
    if (ImGui::DragFloat("Fog Density", &density, 0.001f, 0.0f, 1.0f, "%.3f"))
    {
        FogComponent->SetFogDensity(density);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("안개의 농도를 조절합니다. 값이 클수록 안개가 짙어집니다.");
    }

    // FogHeightFalloff
    float heightFalloff = FogComponent->GetFogHeightFalloff();
    if (ImGui::DragFloat("Height Falloff", &heightFalloff, 0.001f, 0.0f, 1.0f, "%.3f"))
    {
        FogComponent->SetFogHeightFalloff(heightFalloff);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("높이에 따른 안개 감쇠율을 설정합니다. 값이 클수록 높이에 따라 안개가 빠르게 옅어집니다.");
    }

    // StartDistance
    float startDistance = FogComponent->GetStartDistance();
    if (ImGui::DragFloat("Start Distance", &startDistance, 0.5f, 0.0f, FLT_MAX, "%.1f"))
    {
        FogComponent->SetStartDistance(startDistance);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("카메라로부터 안개가 시작되는 거리를 설정합니다.");
    }

    // FogCutoffDistance
    float cutoffDistance = FogComponent->GetFogCutoffDistance();
    if (ImGui::DragFloat("Cutoff Distance", &cutoffDistance, 10.0f, 0.0f, FLT_MAX, "%.1f"))
    {
        FogComponent->SetFogCutoffDistance(cutoffDistance);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("안개 효과가 최대가 되는 거리를 설정합니다. 이 거리 이후로는 안개가 더 이상 짙어지지 않습니다.");
    }

    // FogMaxOpacity
    float maxOpacity = FogComponent->GetFogMaxOpacity();
    if (ImGui::DragFloat("Max Opacity", &maxOpacity, 0.01f, 0.0f, 1.0f, "%.2f"))
    {
        FogComponent->SetFogMaxOpacity(maxOpacity);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("안개의 최대 불투명도를 설정합니다. 0은 완전 투명, 1은 완전 불투명입니다.");
    }

    // FogInscatteringColor
    FVector color = FogComponent->GetFogInscatteringColor();
    float colorArr[3] = { color.X, color.Y, color.Z };
    if (ImGui::ColorEdit3("Inscattering Color", colorArr))
    {
        FogComponent->SetFogInscatteringColor(FVector(colorArr[0], colorArr[1], colorArr[2]));
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("안개에 산란되는 빛의 색상을 설정합니다. 안개의 전체적인 색감을 결정합니다.");
    }
    
    ImGui::PopStyleColor(3);
    ImGui::PopItemWidth();
    ImGui::Separator();
}