#include "pch.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/Renderer/Public/Renderer.h"

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
	// 라이트 활성화 체크박스
    bool LightEnabled = PointLightComponent->GetLightEnabled();
    if (ImGui::Checkbox("Light Enabled", &LightEnabled))
    {
        PointLightComponent->SetLightEnabled(LightEnabled);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트를 켜고 끕니다.\n끄면 조명 계산에서 제외됩니다.\n(Outliner O/X와는 별개)");
    }
    // Light Color
    FVector LightColor = PointLightComponent->GetLightColor();
    // Convert from 0-1 to 0-255 for display
    float LightColorRGB[3] = { LightColor.X * 255.0f, LightColor.Y * 255.0f, LightColor.Z * 255.0f };
    
    bool ColorChanged = false;
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    
    float BoxWidth = 65.0f;  // Fixed width for each RGB box
    
    // R channel
    ImGui::SetNextItemWidth(BoxWidth);
    ImVec2 PosR = ImGui::GetCursorScreenPos();
    ColorChanged |= ImGui::DragFloat("##R", &LightColorRGB[0], 1.0f, 0.0f, 255.0f, "R: %.0f");
    ImVec2 SizeR = ImGui::GetItemRectSize();
    DrawList->AddLine(ImVec2(PosR.x + 5, PosR.y + 2), ImVec2(PosR.x + 5, PosR.y + SizeR.y - 2), IM_COL32(255, 0, 0, 255), 2.0f);
    
    ImGui::SameLine();
    
    // G channel
    ImGui::SetNextItemWidth(BoxWidth);
    ImVec2 PosG = ImGui::GetCursorScreenPos();
    ColorChanged |= ImGui::DragFloat("##G", &LightColorRGB[1], 1.0f, 0.0f, 255.0f, "G: %.0f");
    ImVec2 SizeG = ImGui::GetItemRectSize();
    DrawList->AddLine(ImVec2(PosG.x + 5, PosG.y + 2), ImVec2(PosG.x + 5, PosG.y + SizeG.y - 2), IM_COL32(0, 255, 0, 255), 2.0f);
    
    ImGui::SameLine();
    
    // B channel
    ImGui::SetNextItemWidth(BoxWidth);
    ImVec2 PosB = ImGui::GetCursorScreenPos();
    ColorChanged |= ImGui::DragFloat("##B", &LightColorRGB[2], 1.0f, 0.0f, 255.0f, "B: %.0f");
    ImVec2 SizeB = ImGui::GetItemRectSize();
    DrawList->AddLine(ImVec2(PosB.x + 5, PosB.y + 2), ImVec2(PosB.x + 5, PosB.y + SizeB.y - 2), IM_COL32(0, 0, 255, 255), 2.0f);
    
    ImGui::SameLine();
    
    // Color picker button
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
        // Convert back from 0-255 to 0-1
        LightColor.X = LightColorRGB[0] / 255.0f;
        LightColor.Y = LightColorRGB[1] / 255.0f;
        LightColor.Z = LightColorRGB[2] / 255.0f;
        PointLightComponent->SetLightColor(LightColor);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트 필터 색입니다.\n이 색을 조절하면 실제 라이트의 강도가 조절되는 것과 같은 효과가 생까게 됩니다.");
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

    /*
     * 그림자 속성 관련 UI
     */
    ImGui::Separator();
    
    bool CastShadow = PointLightComponent->GetCastShadows();
    if (ImGui::Checkbox("Cast Shadow", &CastShadow))
    {
        PointLightComponent->SetCastShadows(CastShadow);
    }

    if (PointLightComponent->GetCastShadows())
    {
        float ShadowResoulutionScale = PointLightComponent->GetShadowResolutionScale();
        if (ImGui::DragFloat("ShadowResoulutionScale", &ShadowResoulutionScale, 0.1f, 0.0f, 20.0f))
        {
            PointLightComponent->SetShadowResolutionScale(ShadowResoulutionScale);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 해상도 크기\n범위: 0.0(최소) ~ 20.0(최대)");
        }

        float ShadowBias = PointLightComponent->GetShadowBias();
        if (ImGui::DragFloat("ShadowBias", &ShadowBias, 0.1f, 0.0f, 20.0f))
        {
            PointLightComponent->SetShadowBias(ShadowBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 깊이 보정\n범위: 0.0(최소) ~ 20.0(최대)");
        }

        float ShadowSlopeBias = PointLightComponent->GetShadowSlopeBias();
        if (ImGui::DragFloat("ShadowSlopeBias", &ShadowSlopeBias, 0.1f, 0.0f, 20.0f))
        {
            PointLightComponent->SetShadowSlopeBias(ShadowSlopeBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 기울기 비례 보정\n범위: 0.0(최소) ~ 20.0(최대)");
        }

        float ShadowSharpen = PointLightComponent->GetShadowSharpen();
        if (ImGui::DragFloat("ShadowSharpen", &ShadowSharpen, 0.1f, 0.0f, 20.0f))
        {
            PointLightComponent->SetShadowSharpen(ShadowSharpen);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 첨도(Sharpness)\n범위: 0.0(최소) ~ 20.0(최대)");
        }

		// Shadow Mode
		EShadowModeIndex CurrentShadowMode = PointLightComponent->GetShadowModeIndex();
		const char* ShadowModeNames[] = { "UnFiltered", "PCF", "VSM", "VSM_BOX", "VSM_GAUSSIAN", "SAVSM" };
		const char* CurrentShadowModeName = ShadowModeNames[static_cast<int>(CurrentShadowMode)];

		if (ImGui::BeginCombo("Shadow Mode", CurrentShadowModeName))
		{
			for (int i = 0; i < IM_ARRAYSIZE(ShadowModeNames); ++i)
			{
				bool bIsSelected = (CurrentShadowMode == static_cast<EShadowModeIndex>(i));
				if (ImGui::Selectable(ShadowModeNames[i], bIsSelected))
				{
					PointLightComponent->SetShadowModeIndex(static_cast<EShadowModeIndex>(i));
				}

				if (bIsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

        float NearPlane = PointLightComponent->GetNearPlane();
        if (ImGui::DragFloat("NearPlane", &NearPlane, 0.1f, 0.0f, 1.0f))
        {
            PointLightComponent->SetNearPlane(NearPlane);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("광원 시야 근평면\n범위: 0.0(최소) ~ 1.0(최대)");
        }

        float FarPlane = PointLightComponent->GetFarPlane();
        if (ImGui::DragFloat("FarPlane", &FarPlane, 0.1f, 10.0f, 1000.0f))
        {
            PointLightComponent->SetFarPlane(FarPlane);
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
                Camera->SetLocation(PointLightComponent->GetWorldLocation());
                Camera->SetRotation(PointLightComponent->GetWorldRotation());
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

    ImTextureID TextureID = (ImTextureID)URenderer::GetInstance().GetDeviceResources()->GetNormalSRV();
    if (URenderer::GetInstance().GetDeviceResources()->GetNormalSRV())
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

