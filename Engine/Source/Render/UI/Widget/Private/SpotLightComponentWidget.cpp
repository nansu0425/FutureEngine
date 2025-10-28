#include "pch.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"
#include "Component/Public/SpotLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Level/Public/Level.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"

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
	// 라이트 활성화 체크박스
    bool LightEnabled = SpotLightComponent->GetLightEnabled();
    if (ImGui::Checkbox("Light Enabled", &LightEnabled))
    {
        SpotLightComponent->SetLightEnabled(LightEnabled);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트를 켜고 끕니다.\n끄면 조명 계산에서 제외됩니다.\n(Outliner O/X와는 별개)");
    }
    // Light Color
    FVector LightColor = SpotLightComponent->GetLightColor();
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
        SpotLightComponent->SetLightColor(LightColor);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("라이트 필터 색입니다.\n이 색을 조절하면 실제 라이트의 강도가 조절되는 것과 같은 효과가 생기게 됩니다.");
    }

    // Intensity
    float Intensity = SpotLightComponent->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 0.1f, 0.0f, 20.0f))
    {
        SpotLightComponent->SetIntensity(Intensity);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("스포트라이트 밝기\n범위: 0.0(꺼짐) ~ 20.0(최대)");
    }

    // Distance Falloff Exponent
    float DistanceFalloffExponent = SpotLightComponent->GetDistanceFalloffExponent();
    if (ImGui::DragFloat("Distance Falloff Exponent", &DistanceFalloffExponent, 0.1f, 2.0f, 16.0f))
    {
        SpotLightComponent->SetDistanceFalloffExponent(DistanceFalloffExponent);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("거리에 따라 밝기가 줄어드는 속도를 조절합니다.\n값이 클수록 감소가 더 급격합니다.");
    }

    // Angle Falloff Exponent
    float AngleFalloffExponent = SpotLightComponent->GetAngleFalloffExponent();
    if (ImGui::DragFloat("Angle Falloff Exponent", &AngleFalloffExponent, 0.5f, 1.0f, 128.0f))
    {
        SpotLightComponent->SetAngleFalloffExponent(AngleFalloffExponent);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("내부 원뿔에서 외부 원뿔로 밝기가 바뀌는 부드러움을 조절합니다.\n값이 클수록 가장자리가 더 또렷해집니다.");
    }

    // Attenuation Radius
    float AttenuationRadius = SpotLightComponent->GetAttenuationRadius();
    if (ImGui::DragFloat("Attenuation Radius", &AttenuationRadius, 0.1f, 0.0f, 1000.0f))
    {
        SpotLightComponent->SetAttenuationRadius(AttenuationRadius);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("빛이 닿는 최대 거리입니다.\n이 반경에서 밝기는 0으로 떨어집니다.");
    }

    // Outer Cone Angle
    float OuterAngleDegrees = SpotLightComponent->GetOuterConeAngle() * ToDeg;
    if (ImGui::DragFloat("Outer Cone Angle (deg)", &OuterAngleDegrees, 1.0f, 0.0f, 89.0f))
    {
        SpotLightComponent->SetOuterAngle(OuterAngleDegrees * ToRad);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("스포트라이트 원뿔의 바깥쪽 가장자리 각도입니다.\n이 각도 바깥은 완전히 어둡습니다.");
    }

    // Inner Cone Angle
    float InnerAngleDegrees = SpotLightComponent->GetInnerConeAngle() * ToDeg;
    if (ImGui::DragFloat("Inner Cone Angle (deg)", &InnerAngleDegrees, 1.0f, 0.0f, OuterAngleDegrees))
    {
        SpotLightComponent->SetInnerAngle(InnerAngleDegrees * ToRad);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("스포트라이트 원뿔의 안쪽 가장자리 각도입니다.\n이 각도 안쪽은 최대 밝기입니다.");
    }

    /*
     * 그림자 속성 관련 UI
     */
    ImGui::Separator();
    
    bool CastShadow = SpotLightComponent->GetCastShadows();
    if (ImGui::Checkbox("Cast Shadow", &CastShadow))
    {
        SpotLightComponent->SetCastShadows(CastShadow);
    }

    if (SpotLightComponent->GetCastShadows())
    {
        // Shadow Resolution 선택용 ComboBox
        const int shadowResOptions[] = {128, 256, 512, 1024};
        const char* shadowResLabels[] = {"128", "256", "512", "1024"};

        // 현재 라이트의 ShadowResolutionScale을 읽음
        float currentScale = SpotLightComponent->GetShadowResolutionScale();

        // 현재 값과 가장 가까운 옵션 인덱스 계산
        int currentIndex = 0;
        for (int i = 0; i < IM_ARRAYSIZE(shadowResOptions); ++i)
        {
            if (fabs(currentScale - static_cast<float>(shadowResOptions[i])) < 1e-3f)
            {
                currentIndex = i;
                break;
            }
        }
        
        // ComboBox 생성
        if (ImGui::Combo("Shadow Resolution", &currentIndex, shadowResLabels, IM_ARRAYSIZE(shadowResLabels)))
        {
            // 선택된 값 적용
            SpotLightComponent->SetShadowResolutionScale(static_cast<float>(shadowResOptions[currentIndex]));
        }

        // 툴팁 표시
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 해상도 크기\n옵션: 128, 256, 512, 1024");
        }

        float ShadowBias = SpotLightComponent->GetShadowBias();
        if (ImGui::DragFloat("ShadowBias", &ShadowBias, 0.005f, 0.0f, 0.1f))
        {
            SpotLightComponent->SetShadowBias(ShadowBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 깊이 보정\n범위: 0.0(최소) ~ 0.1(최대)");
        }

        float ShadowSlopeBias = SpotLightComponent->GetShadowSlopeBias();
        if (ImGui::DragFloat("ShadowSlopeBias", &ShadowSlopeBias, 0.1f, 0.0f, 4.0f))
        {
            SpotLightComponent->SetShadowSlopeBias(ShadowSlopeBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 기울기 비례 보정\n범위: 0.0(최소) ~ 4.0(최대)");
        }

        float ShadowSharpen = SpotLightComponent->GetShadowSharpen();
        if (ImGui::DragFloat("ShadowSharpen", &ShadowSharpen, 0.05f, 0.0f, 1.0f))
        {
            SpotLightComponent->SetShadowSharpen(ShadowSharpen);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 첨도(Sharpness)\n범위: 0.0(최소) ~ 1.0(최대)");
        }

		// Shadow Mode
		EShadowModeIndex CurrentShadowMode = SpotLightComponent->GetShadowModeIndex();
		const char* ShadowModeNames[] = { "UnFiltered", "PCF", "VSM", "VSM_BOX", "VSM_GAUSSIAN", "SAVSM" };
		const char* CurrentShadowModeName = ShadowModeNames[static_cast<int>(CurrentShadowMode)];

		if (ImGui::BeginCombo("Shadow Mode", CurrentShadowModeName))
		{
			for (int i = 0; i < IM_ARRAYSIZE(ShadowModeNames); ++i)
			{
				bool bIsSelected = (CurrentShadowMode == static_cast<EShadowModeIndex>(i));
				if (ImGui::Selectable(ShadowModeNames[i], bIsSelected))
				{
					SpotLightComponent->SetShadowModeIndex(static_cast<EShadowModeIndex>(i));
				}

				if (bIsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

        float NearPlane = SpotLightComponent->GetNearPlane();
        if (ImGui::DragFloat("NearPlane", &NearPlane, 0.1f, 0.0f, 1.0f))
        {
            SpotLightComponent->SetNearPlane(NearPlane);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("광원 시야 근평면\n범위: 0.0(최소) ~ 1.0(최대)");
        }

        float FarPlane = SpotLightComponent->GetFarPlane();
        if (ImGui::DragFloat("FarPlane", &FarPlane, 0.1f, 10.0f, 1000.0f))
        {
            SpotLightComponent->SetFarPlane(FarPlane);
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
                Camera->SetLocation(SpotLightComponent->GetWorldLocation());
                Camera->SetRotation(SpotLightComponent->GetWorldRotation());
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

    ID3D11ShaderResourceView* ShadowSRV = URenderer::GetInstance().GetShadowMapPass()->GetShadowAtlas()->ShadowSRV.Get();
    ImTextureID TextureID = (ImTextureID)ShadowSRV;
    if (ShadowSRV)
    {
        FRenderingContext RenderingContext = URenderer::GetInstance().GetRenderingContext();
        int32 SpotLightIdx = 0;
        for (USpotLightComponent* SpotLight : RenderingContext.SpotLights)
        {
            if (SpotLightComponent == SpotLight)
                break;
            SpotLightIdx++;
        }

        const static int32 IDX_MAX = 8;

        if (SpotLightIdx < IDX_MAX)
        {
            // 원하는 출력 크기 설정
            ImVec2 ImageSize(256, 256); 

            ImVec2 startPos(0.125f * static_cast<float>(SpotLightIdx), 0.125f);
            ImVec2 endPos = startPos + ImVec2(
            1.0f / (8192.0f / SpotLightComponent->GetShadowResolutionScale()),
            1.0f / (8192.0f / SpotLightComponent->GetShadowResolutionScale())
            );
            
            // ImGui::Image(텍스처 ID, 크기, UV 시작점, UV 끝점, Tint Color, Border Color)
            // 일반적으로 (0,0)에서 (1,1)까지의 UV를 사용하고, Tint Color는 흰색, Border Color는 투명으로 설정합니다.
            ImGui::Image(TextureID, 
                         ImageSize, 
                         startPos,
                         endPos, 
                         ImVec4(1, 1, 1, 1), 
                         ImVec4(0, 0, 0, 0)); 

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("광원의 Shadow Map 출력");
            }
        }
    }
    
    ImGui::PopStyleColor(3);

    ImGui::Separator();
}
