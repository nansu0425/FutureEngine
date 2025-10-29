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
        // Shadow Resolution 선택용 ComboBox
        const int shadowResOptions[] = {128, 256, 512, 1024};
        const char* shadowResLabels[] = {"128", "256", "512", "1024"};

        // 현재 라이트의 ShadowResolutionScale을 읽음
        float currentScale = DirectionalLightComponent->GetShadowResolutionScale();

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
            DirectionalLightComponent->SetShadowResolutionScale(static_cast<float>(shadowResOptions[currentIndex]));
        }

        // 툴팁 표시
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 해상도 크기\n옵션: 128, 256, 512, 1024");
        }

        float ShadowBias = DirectionalLightComponent->GetShadowBias();
        if (ImGui::DragFloat("ShadowBias", &ShadowBias, 0.005f, 0.0f, 0.1f))
        {
            DirectionalLightComponent->SetShadowBias(ShadowBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 깊이 보정\n범위: 0.0(최소) ~ 0.1(최대)");
        }

        float ShadowSlopeBias = DirectionalLightComponent->GetShadowSlopeBias();
        if (ImGui::DragFloat("ShadowSlopeBias", &ShadowSlopeBias, 0.1f, 0.0f, 4.0f))
        {
            DirectionalLightComponent->SetShadowSlopeBias(ShadowSlopeBias);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 기울기 비례 보정\n범위: 0.0(최소) ~ 4.0(최대)");
        }

        float ShadowSharpen = DirectionalLightComponent->GetShadowSharpen();
        if (ImGui::DragFloat("ShadowSharpen", &ShadowSharpen, 0.05f, 0.0f, 1.0f))
        {
            DirectionalLightComponent->SetShadowSharpen(ShadowSharpen);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("그림자 첨도(Sharpness)\n범위: 0.0(최소) ~ 1.0(최대)");
        }

		// Shadow Mode
		EShadowModeIndex CurrentShadowMode = DirectionalLightComponent->GetShadowModeIndex();
		const char* ShadowModeNames[] = { "UnFiltered", "PCF", "VSM", "VSM_BOX", "VSM_GAUSSIAN", "SAVSM" };
		const char* CurrentShadowModeName = ShadowModeNames[static_cast<int>(CurrentShadowMode)];

		if (ImGui::BeginCombo("Shadow Mode", CurrentShadowModeName))
		{
			for (int i = 0; i < IM_ARRAYSIZE(ShadowModeNames); ++i)
			{
				bool bIsSelected = (CurrentShadowMode == static_cast<EShadowModeIndex>(i));
				if (ImGui::Selectable(ShadowModeNames[i], bIsSelected))
				{
					DirectionalLightComponent->SetShadowModeIndex(static_cast<EShadowModeIndex>(i));
				}

				if (bIsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// 그림자 매핑 방식 선택
		ImGui::Separator();
		ImGui::Text("그림자 매핑 방식");

		uint8 CurrentProjectionMode = DirectionalLightComponent->GetShadowProjectionMode();
		const char* ProjectionModeNames[] = { "Uniform SM (단일)", "PSM (단일)", "CSM (캐스케이드)" };
		const char* CurrentProjectionModeName = ProjectionModeNames[CurrentProjectionMode];

		if (ImGui::BeginCombo("투영 방식", CurrentProjectionModeName))
		{
			for (int i = 0; i < IM_ARRAYSIZE(ProjectionModeNames); ++i)
			{
				bool bIsSelected = (CurrentProjectionMode == i);
				if (ImGui::Selectable(ProjectionModeNames[i], bIsSelected))
				{
					DirectionalLightComponent->SetShadowProjectionMode(static_cast<uint8>(i));
				}

				if (bIsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("그림자 매핑 방식:\n\n"
				"[0] Uniform SM (단일): 단일 직교 투영 그림자 맵\n"
				"    - 간단하고 안정적\n"
				"    - 품질: 낮음\n\n"
				"[1] PSM (단일): 단일 원근 투영 그림자 맵\n"
				"    - 카메라 근처 해상도 향상\n"
				"    - 품질: 중간, 실험적\n\n"
				"[2] CSM (캐스케이드): 다중 Uniform 그림자 맵\n"
				"    - 거리별 캐스케이드 분할 (8개)\n"
				"    - 품질: 높음, 추천");
		}

		// PSM 전용 설정 (PSM 선택 시에만 표시)
		if (CurrentProjectionMode == 1)  // PSM
		{
			ImGui::Indent();

			bool bSlideBackEnabled = DirectionalLightComponent->GetPSMSlideBackEnabled();
			if (ImGui::Checkbox("슬라이드 백 활성화", &bSlideBackEnabled))
			{
				DirectionalLightComponent->SetPSMSlideBackEnabled(bSlideBackEnabled);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("가상 카메라를 뒤로 밀어서 무한 평면 특이점 회피");
			}

			if (bSlideBackEnabled)
			{
				float MinInfinityZ = DirectionalLightComponent->GetPSMMinInfinityZ();
				if (ImGui::DragFloat("최소 무한 Z", &MinInfinityZ, 0.1f, 1.0f, 10.0f))
				{
					DirectionalLightComponent->SetPSMMinInfinityZ(MinInfinityZ);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("무한 평면까지의 최소 거리");
				}
			}

			bool bUnitCubeClip = DirectionalLightComponent->GetPSMUnitCubeClip();
			if (ImGui::Checkbox("유닛 큐브 클리핑", &bUnitCubeClip))
			{
				DirectionalLightComponent->SetPSMUnitCubeClip(bUnitCubeClip);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("보이는 리시버에 맞춰 그림자 맵 최적화");
			}

			ImGui::Unindent();
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

    ID3D11ShaderResourceView* ShadowSRV = URenderer::GetInstance().GetShadowMapPass()->GetShadowAtlas()->ShadowSRV.Get();
    ImTextureID TextureID = (ImTextureID)ShadowSRV;

    if (ShadowSRV)
    {
        UCascadeManager& CascadeManager = UCascadeManager::GetInstance();
        int splitNum = CascadeManager.GetSplitNum();

        static int currentCascade = 0;
        ImGui::Text("Cascade SubFrustum Number");
        ImGui::SliderInt("##CascadeSlider", &currentCascade, 0, splitNum - 1);

        ImVec2 imageSize(256, 256);
        ImVec2 startPos(0.125f * currentCascade, 0.0f);
        ImVec2 endPos = startPos + ImVec2(
            1.0f / (8192.0f / DirectionalLightComponent->GetShadowResolutionScale()),
            1.0f / (8192.0f / DirectionalLightComponent->GetShadowResolutionScale())
            );

        ImGui::Image(TextureID, imageSize, startPos, endPos);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("광원의 Shadow Map 출력");
    }
    
    ImGui::PopStyleColor(3);

    ImGui::Separator();
}
