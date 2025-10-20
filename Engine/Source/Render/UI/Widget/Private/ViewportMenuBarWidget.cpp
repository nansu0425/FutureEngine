#include "pch.h"
#include "Render/UI/Widget/Public/ViewportMenuBarWidget.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"
#include "Editor/Public/Editor.h"
#include "Manager/UI/Public/ViewportManager.h"

/* *
* @brief UI에 적용할 색상을 정의합니다.
*/
// 카메라 모드 제어 위젯
const ImVec4 MenuBarBg = ImVec4(pow(0.12f, 2.2f), pow(0.12f, 2.2f), pow(0.12f, 2.2f), 1.00f);
const ImVec4 PopupBg = ImVec4(pow(0.09f, 2.2f), pow(0.09f, 2.2f), pow(0.09f, 2.2f), 1.00f);
const ImVec4 Header = ImVec4(pow(0.20f, 2.2f), pow(0.20f, 2.2f), pow(0.20f, 2.2f), 1.00f);
const ImVec4 HeaderHovered = ImVec4(pow(0.35f, 2.2f), pow(0.35f, 2.2f), pow(0.35f, 2.2f), 1.00f);
const ImVec4 HeaderActive = ImVec4(pow(0.50f, 2.2f), pow(0.50f, 2.2f), pow(0.50f, 2.2f), 1.00f);
const ImVec4 TextColor = ImVec4(pow(0.92f, 2.2f), pow(0.92f, 2.2f), pow(0.92f, 2.2f), 1.00f);
// 카메라 변수 제어 위젯
const ImVec4 FrameBg = ImVec4(pow(0.1f, 2.2f), pow(0.1f, 2.2f), pow(0.1f, 2.2f), 1.00f);
const ImVec4 FrameBgHovered = ImVec4(pow(0.25f, 2.2f), pow(0.25f, 2.2f), pow(0.25f, 2.2f), 1.00f);
const ImVec4 SliderGrab = ImVec4(pow(0.6f, 2.2f), pow(0.6f, 2.2f), pow(0.6f, 2.2f), 1.00f);
const ImVec4 SliderGrabActive = ImVec4(pow(0.8f, 2.2f), pow(0.8f, 2.2f), pow(0.8f, 2.2f), 1.00f);

UViewportMenuBarWidget::UViewportMenuBarWidget()
{
	Editor = GEditor->GetEditorModule();
}

UViewportMenuBarWidget::~UViewportMenuBarWidget()
{
	Viewport = nullptr;
}

void UViewportMenuBarWidget::RenderWidget()
{
	if (!Viewport) { return; }

	//TArray<FViewportClient>& ViewportClients = UViewportManager::GetInstance().GetViewportClient();
	auto& ViewportManager = UViewportManager::GetInstance();
	for (int Index = 0; Index < ViewportManager.GetViewports().size(); ++Index)
	{
		// FutureEngine 철학: Viewport가 Client를 소유, Client가 Camera를 관리
		FViewport* CurrentViewport = ViewportManager.GetViewports()[Index];
		FViewportClient* ViewportClient = CurrentViewport->GetViewportClient();
		const D3D11_VIEWPORT& ViewportInfo = CurrentViewport->GetRenderRect();

		// 뷰포트 영역이 너무 작으면 렌더링하지 않음
		if (ViewportInfo.Width < 1.0f || ViewportInfo.Height < 1.0f) { continue; }

		// 0. 고유 ID 부여 및 스타일 적용
		ImGui::PushID(Index);
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, MenuBarBg);
		ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupBg);
		ImGui::PushStyleColor(ImGuiCol_Header, Header);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HeaderHovered);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, HeaderActive);
		ImGui::PushStyleColor(ImGuiCol_Button, Header);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HeaderHovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, HeaderActive);
		ImGui::PushStyleColor(ImGuiCol_Text, TextColor);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, FrameBg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, FrameBgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, FrameBg);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, SliderGrab);
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, SliderGrabActive);

		// 1. 독립 윈도우 위치와 크기 지정
		ImGui::SetNextWindowPos(ImVec2(ViewportInfo.TopLeftX, ViewportInfo.TopLeftY));
		ImGui::SetNextWindowSize(ImVec2(ViewportInfo.Width, 22.0f));

		std::string WindowName = "ViewportMenuBarContainer_" + std::to_string(Index);

		// 2. 투명한 윈도우 생성 (메뉴바 전용)
		ImGui::Begin(WindowName.c_str(),
			nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBackground |   // 배경 제거
			ImGuiWindowFlags_NoDecoration |   // 테두리 제거 (NoTitleBar, NoResize, NoMove 포함)
			ImGuiWindowFlags_NoBringToFrontOnFocus
		);

		if (ImGui::BeginMenuBar())
		{
			// 3. 기존의 뷰포트 타입 메뉴 (Perspective, Ortho 등)
			// KTLWeek07: ViewportClient가 EViewType을 관리
			EViewType CurrentViewType = ViewportClient->GetViewType();
			const char* ViewTypeNames[] = { "Perspective", "OrthoTop", "OrthoBottom", "OrthoLeft", "OrthoRight", "OrthoFront", "OrthoBack" };
			const char* CurrentViewTypeName = (CurrentViewType < EViewType::OrthoBack) ? ViewTypeNames[(int)CurrentViewType] : "Unknown";

			if (ImGui::BeginMenu(CurrentViewTypeName))
			{
				if (ImGui::MenuItem("Perspective"))
				{
					ViewportClient->SetViewType(EViewType::Perspective);
					if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Perspective); }
				}

				if (ImGui::BeginMenu("Orthographic"))
				{
					if (ImGui::MenuItem("Top")) { ViewportClient->SetViewType(EViewType::OrthoTop); if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Orthographic); } }
					if (ImGui::MenuItem("Bottom")) { ViewportClient->SetViewType(EViewType::OrthoBottom); if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Orthographic); } }
					if (ImGui::MenuItem("Left")) { ViewportClient->SetViewType(EViewType::OrthoLeft); if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Orthographic); } }
					if (ImGui::MenuItem("Right")) { ViewportClient->SetViewType(EViewType::OrthoRight); if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Orthographic); } }
					if (ImGui::MenuItem("Front")) { ViewportClient->SetViewType(EViewType::OrthoFront); if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Orthographic); } }
					if (ImGui::MenuItem("Back")) { ViewportClient->SetViewType(EViewType::OrthoBack); if (UCamera* Cam = ViewportClient->GetCamera()) { Cam->SetCameraType(ECameraType::ECT_Orthographic); } }
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			// 카메라 설정 버튼
			if (ImGui::Button("Camera Settings"))
			{
				ImGui::OpenPopup("CameraSettingsPopup");
			}
			if (ImGui::BeginPopup("CameraSettingsPopup"))
			{
				// FutureEngine 철학: ViewportClient->GetCamera()로 접근
				if (UCamera* Camera = ViewportClient->GetCamera())
				{
					RenderCameraControls(*Camera);
				}
				ImGui::EndPopup();
			}

			ImGui::Separator();

			// 뷰포트 레이아웃 전환 버튼
			{
				const char* LayoutIcon = bIsSingleViewportClient ? "[+]" : "□";
				const char* TooltipText = bIsSingleViewportClient ? "Switch to 4 Viewport" : "Switch to Single Viewport";
				const float ButtonWidth = 25.0f;

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ButtonWidth);

				if (ImGui::Button(LayoutIcon, ImVec2(ButtonWidth, 0)))
				{
					// FutureEngine 철학: ViewportManager로 레이아웃 제어
					bIsSingleViewportClient = !bIsSingleViewportClient;
					if (bIsSingleViewportClient)
					{
						// Single 모드로 전환
						ViewportManager.PersistSplitterRatios();
						ViewportManager.StartLayoutAnimation(false, Index);
					}
					else
					{
						// Quad 모드로 전환
						ViewportManager.StartLayoutAnimation(true, Index);
					}
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip(TooltipText);
				}
			}
			ImGui::EndMenuBar();
		}

		ImGui::End(); // End of ViewportMenuBarContainer 윈도우

		ImGui::PopStyleColor(14);
		ImGui::PopID();
	}
}

void UViewportMenuBarWidget::RenderCameraControls(UCamera& InCamera)
{
	// --- UI를 그리기 직전에 항상 카메라로부터 최신 값을 가져옵니다 ---
	auto& Location = InCamera.GetLocation();
	auto& Rotation = InCamera.GetRotation();
	float FovY = InCamera.GetFovY();
	float NearZ = InCamera.GetNearZ();
	float FarZ = InCamera.GetFarZ();
	float OrthoWidth = InCamera.GetOrthoWidth();
	float MoveSpeed = InCamera.GetMoveSpeed();
	int ModeIndex = (InCamera.GetCameraType() == ECameraType::ECT_Perspective) ? 0 : 1;
	static const char* CameraMode[] = { "Perspective", "Orthographic" };


	ImGui::Text("Camera Properties");
	ImGui::Separator();

	// --- UI 렌더링 및 상호작용 ---
	if (ImGui::SliderFloat("Move Speed", &MoveSpeed, UCamera::MIN_SPEED, UCamera::MAX_SPEED, "%.1f"))
	{
		InCamera.SetMoveSpeed(MoveSpeed); // 변경 시 즉시 적용
	}

	ImGui::DragFloat3("Location", &Location.X, 0.05f);
	ImGui::DragFloat3("Rotation", &Rotation.X, 0.1f);

	bool bOpticsChanged = false;
	if (ModeIndex == 0) // 원근 투영 
	{
		bOpticsChanged |= ImGui::SliderFloat("FOV", &FovY, 1.0f, 170.0f, "%.1f");
		bOpticsChanged |= ImGui::DragFloat("Z Near", &NearZ, 0.01f, 0.0001f, 1e6f, "%.4f");
		bOpticsChanged |= ImGui::DragFloat("Z Far", &FarZ, 0.1f, 0.001f, 1e7f, "%.3f");
	}
	else if (ModeIndex == 1) // 직교 투영
	{
		bOpticsChanged |= ImGui::SliderFloat("OrthoWidth", &OrthoWidth, 1.0f, 150.0f, "%.1f");
	}

	// 변경된 값을 카메라에 다시 적용
	if (bOpticsChanged)
	{
		InCamera.SetFovY(FovY);
		InCamera.SetNearZ(NearZ);
		InCamera.SetFarZ(FarZ);
		InCamera.SetOrthoWidth(OrthoWidth);
	}
}
