#include "pch.h"
#include "Render/UI/Widget/Public/DirectionalLightComponentWidget.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"

IMPLEMENT_CLASS(UDirectionalLightComponentWidget, UWidget)

void UDirectionalLightComponentWidget::Initialize()
{
}

void UDirectionalLightComponentWidget::Update()
{
    ULevel* CurrentLevel = GWorld->GetLevel();
    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();
        if (DirectionalLightComponent != NewSelectedComponent)
        {
            DirectionalLightComponent = Cast<UDirectionalLightComponent>(NewSelectedComponent);
        }
    }
}

void UDirectionalLightComponentWidget::RenderWidget()
{
    if (!DirectionalLightComponent)
    {
        return;
    }

    ImGui::Separator();
    
    // 모든 입력 필드를 검은색으로 설정
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	// 라이트 활성화 체크박스
    bool LightEnabled = DirectionalLightComponent->GetLightEnabled();
    if (ImGui::Checkbox("Light Enabled", &LightEnabled))
    {
        DirectionalLightComponent->SetLightEnabled(LightEnabled);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트를 켜고 끕니다.\n끄면 조명 계산에서 제외됩니다.\n(Outliner O/X와는 별개)");
    }
    // Light Color
    FVector LightColor = DirectionalLightComponent->GetLightColor();
    float LightColorRGB[3] = { LightColor.X * 255.0f, LightColor.Y * 255.0f, LightColor.Z * 255.0f };
    
    bool ColorChanged = false;
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    float BoxWidth = 65.0f;
    
    ImGui::SetNextItemWidth(BoxWidth);
    ImVec2 PosR = ImGui::GetCursorScreenPos();
    ColorChanged |= ImGui::DragFloat("##R", &LightColorRGB[0], 1.0f, 0.0f, 255.0f, "R: %.0f");
    ImVec2 SizeR = ImGui::GetItemRectSize();
    DrawList->AddLine(ImVec2(PosR.x + 5, PosR.y + 2), ImVec2(PosR.x + 5, PosR.y + SizeR.y - 2), IM_COL32(255, 0, 0, 255), 2.0f);
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(BoxWidth);
    ImVec2 PosG = ImGui::GetCursorScreenPos();
    ColorChanged |= ImGui::DragFloat("##G", &LightColorRGB[1], 1.0f, 0.0f, 255.0f, "G: %.0f");
    ImVec2 SizeG = ImGui::GetItemRectSize();
    DrawList->AddLine(ImVec2(PosG.x + 5, PosG.y + 2), ImVec2(PosG.x + 5, PosG.y + SizeG.y - 2), IM_COL32(0, 255, 0, 255), 2.0f);
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(BoxWidth);
    ImVec2 PosB = ImGui::GetCursorScreenPos();
    ColorChanged |= ImGui::DragFloat("##B", &LightColorRGB[2], 1.0f, 0.0f, 255.0f, "B: %.0f");
    ImVec2 SizeB = ImGui::GetItemRectSize();
    DrawList->AddLine(ImVec2(PosB.x + 5, PosB.y + 2), ImVec2(PosB.x + 5, PosB.y + SizeB.y - 2), IM_COL32(0, 0, 255, 255), 2.0f);
    ImGui::SameLine();
    
    float LightColor01[3] = { LightColorRGB[0] / 255.0f, LightColorRGB[1] / 255.0f, LightColorRGB[2] / 255.0f };
    if (ImGui::ColorEdit3("Light Color", LightColor01, ImGuiColorEditFlags_NoInputs))
    {
        LightColorRGB[0] = LightColor01[0] * 255.0f;
        LightColorRGB[1] = LightColor01[1] * 255.0f;
        LightColorRGB[2] = LightColor01[2] * 255.0f;
        ColorChanged = true;
    }
    
    if (ColorChanged)
    {
        LightColor.X = LightColorRGB[0] / 255.0f;
        LightColor.Y = LightColorRGB[1] / 255.0f;
        LightColor.Z = LightColorRGB[2] / 255.0f;
        DirectionalLightComponent->SetLightColor(LightColor);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트 필터 색입니다.\n이 색을 조절하면 실제 라이트의 강도가 조절되는 것과 같은 효과가 생기게 됩니다.");
    }

    float Intensity = DirectionalLightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        DirectionalLightComponent->SetIntensity(Intensity);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("디렉셔널 라이트 밝기\n범위: 0.0(꺼짐) ~ 20.0(최대)");
    }

    /*
     * 그림자 속성 관련 UI
     */
    ImGui::Separator();
    
    bool CastShadow = DirectionalLightComponent->GetCastShadows();
    if (ImGui::Checkbox("Cast Shadow", &CastShadow))
    {
        DirectionalLightComponent->SetCastShadows(CastShadow);
    }

    if (DirectionalLightComponent->GetCastShadows())
    {
        float ShadowResoulutionScale = DirectionalLightComponent->GetShadowResolutionScale();
        if (ImGui::DragFloat("ShadowResoulutionScale", &ShadowResoulutionScale, 0.1f, 0.0f, 20.0f))
        {
            DirectionalLightComponent->SetShadowResolutionScale(ShadowResoulutionScale);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 해상도 크기\n범위: 0.0(최소) ~ 20.0(최대)");
        }

        float ShadowBias = DirectionalLightComponent->GetShadowBias();
        if (ImGui::DragFloat("ShadowBias", &ShadowBias, 0.1f, 0.0f, 20.0f))
        {
            DirectionalLightComponent->SetShadowBias(ShadowBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 깊이 보정\n범위: 0.0(최소) ~ 20.0(최대)");
        }

        float ShadowSlopeBias = DirectionalLightComponent->GetShadowSlopeBias();
        if (ImGui::DragFloat("ShadowSlopeBias", &ShadowSlopeBias, 0.1f, 0.0f, 20.0f))
        {
            DirectionalLightComponent->SetShadowSlopeBias(ShadowSlopeBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 기울기 비례 보정\n범위: 0.0(최소) ~ 20.0(최대)");
        }

        float ShadowSharpen = DirectionalLightComponent->GetShadowSharpen();
        if (ImGui::DragFloat("ShadowSharpen", &ShadowSharpen, 0.1f, 0.0f, 20.0f))
        {
            DirectionalLightComponent->SetShadowSharpen(ShadowSharpen);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 첨도(Sharpness)\n범위: 0.0(최소) ~ 20.0(최대)");
        }
    }

    if (ImGui::Button("Override Camera With Light's Perspective"))
    {
        for (FViewport* Viewport : UViewportManager::GetInstance().GetViewports())
        {
            FViewportClient* ViewportClient = Viewport->GetViewportClient();
            if (UCamera* Camera = ViewportClient->GetCamera())
            {
                Camera->SetLocation(DirectionalLightComponent->GetWorldLocation());
                Camera->SetRotation(DirectionalLightComponent->GetWorldRotation());
            }
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("카메라의 시점을 광원의 시점으로 변경\n");
    }

    /*
     * Light Shadow Map 출력 UI
     * 임시로 NormalSRV 출력
     */

    ID3D11ShaderResourceView* ShadowSRV = URenderer::GetInstance().GetShadowMapPass()->GetDirectionalShadowMap(DirectionalLightComponent)->ShadowSRV.Get();
    ImTextureID TextureID = (ImTextureID)ShadowSRV;
    if (ShadowSRV)
    {
        // 원하는 출력 크기 설정
        ImVec2 ImageSize(256, 256); 
    
        // ImGui::Image(텍스처 ID, 크기, UV 시작점, UV 끝점, Tint Color, Border Color)
        // 일반적으로 (0,0)에서 (1,1)까지의 UV를 사용하고, Tint Color는 흰색, Border Color는 투명으로 설정합니다.
        ImGui::Image(TextureID, 
                     ImageSize, 
                     ImVec2(0, 0), ImVec2(1, 1), 
                     ImVec4(1, 1, 1, 1), 
                     ImVec4(0, 0, 0, 0)); 

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("광원의 Shadow Map 출력");
        }
    }
    
    ImGui::PopStyleColor(3);

    ImGui::Separator();
}
