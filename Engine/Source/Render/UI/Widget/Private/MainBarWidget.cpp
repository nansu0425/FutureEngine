#include "pch.h"
#include "Render/UI/Widget/Public/MainBarWidget.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Window/Public/UIWindow.h"
#include "Level/Public/Level.h"
#include <shobjidl.h>

IMPLEMENT_CLASS(UMainBarWidget, UWidget)

/**
 * @brief MainBarWidget 초기화 함수
 * UIManager 인스턴스를 여기서 가져온다
 */
void UMainBarWidget::Initialize()
{
	UIManager = &UUIManager::GetInstance();
	if (!UIManager)
	{
		UE_LOG("MainBarWidget: UIManager를 찾을 수 없습니다!");
		return;
	}

	UE_LOG("MainBarWidget: 메인 메뉴바 위젯이 초기화되었습니다");
}

void UMainBarWidget::Update()
{
	// 업데이트 정보 필요할 경우 추가할 것
}

void UMainBarWidget::RenderWidget()
{
	if (!bIsMenuBarVisible)
	{
		MenuBarHeight = 0.0f;
		return;
	}

	// FutureEngine 스타일: 메뉴바 색상 및 스타일 설정
	const ImVec4 MenuBarBg = ImVec4(pow(0.12f, 2.2f), pow(0.12f, 2.2f), pow(0.12f, 2.2f), 1.0f);
	const ImVec4 PopupBg = ImVec4(pow(0.09f, 2.2f), pow(0.09f, 2.2f), pow(0.09f, 2.2f), 1.0f);
	const ImVec4 Header = ImVec4(pow(0.20f, 2.2f), pow(0.20f, 2.2f), pow(0.20f, 2.2f), 1.0f);
	const ImVec4 HeaderHovered = ImVec4(pow(0.35f, 2.2f), pow(0.35f, 2.2f), pow(0.35f, 2.2f), 1.0f);
	const ImVec4 HeaderActive = ImVec4(pow(0.50f, 2.2f), pow(0.50f, 2.2f), pow(0.50f, 2.2f), 1.0f);
	const ImVec4 TextColor = ImVec4(pow(0.92f, 2.2f), pow(0.92f, 2.2f), pow(0.92f, 2.2f), 1.0f);

	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, MenuBarBg);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupBg);
	ImGui::PushStyleColor(ImGuiCol_Header, Header);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HeaderHovered);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, HeaderActive);
	ImGui::PushStyleColor(ImGuiCol_Text, TextColor);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));

	// FutureEngine 철학: 독립 윈도우로 메뉴바 위치 조정
	const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(screenSize.x, 20.0f));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("MainMenuBarContainer", nullptr, flags))
	{
		if (ImGui::BeginMenuBar())
		{
			MenuBarHeight = ImGui::GetWindowSize().y;

			// FutureEngine 철학: 왼쪽 60px 더미 공간 (로고 자리)
			ImGui::Dummy(ImVec2(60.0f, 0.0f));
			ImGui::SameLine(0.0f, 0.0f);

			// 메뉴 Listing - FutureEngine 스타일
			RenderViewMenu();
			RenderShowFlagsMenu();
			RenderWindowsMenu();
			RenderHelpMenu();

			ImGui::EndMenuBar();
		}
		else
		{
			MenuBarHeight = 0.0f;
		}
		ImGui::End();
	}
	else
	{
		MenuBarHeight = 0.0f;
	}

	// 스타일 복원
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(6);
}


/**
 * @brief Windows 토글 메뉴를 렌더링하는 함수
 * 등록된 MainMenu를 제외한 모든 UIWindow의 토글 옵션을 표시
 */
void UMainBarWidget::RenderWindowsMenu() const
{
	if (ImGui::BeginMenu("창"))
	{
		if (!UIManager)
		{
			ImGui::Text("UIManager를 사용할 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		// 모든 등록된 UIWindow에 대해 토글 메뉴 항목 생성
		const auto& AllWindows = UIManager->GetAllUIWindows();

		if (AllWindows.empty())
		{
			ImGui::Text("등록된 창이 없습니다");
		}
		else
		{
			for (auto* Window : AllWindows)
			{
				if (!Window)
				{
					continue;
				}

				if (Window->GetWindowTitle() == "MainMenuBar")
				{
					continue;
				}

				if (ImGui::MenuItem(Window->GetWindowTitle().ToString().data(), nullptr, Window->IsVisible()))
				{
					Window->ToggleVisibility();

					UE_LOG("MainBarWidget: %s 창 토글됨 (현재 상태: %s)",
						Window->GetWindowTitle().ToString().data(),
						Window->IsVisible() ? "표시" : "숨김");
				}
			}

			ImGui::Separator();

			// 전체 창 제어 옵션
			if (ImGui::MenuItem("모든 창 표시"))
			{
				UIManager->ShowAllWindows();
				UE_LOG("MainBarWidget: 모든 창 표시됨");
			}

			if (ImGui::MenuItem("모든 창 숨김"))
			{
				for (auto* Window : UIManager->GetAllUIWindows())
				{
					if (!Window)
					{
						continue;
					}

					if (Window->GetWindowTitle() == "MainMenuBar")
					{
						continue;
					}

					Window->SetWindowState(EUIWindowState::Hidden);
				}

				UE_LOG("MainBarWidget: 모든 창 숨겨짐");
			}
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief View 메뉴를 렌더링하는 함수
 * ViewMode 선택 기능 (Lit, Unlit, Wireframe)
 */
void UMainBarWidget::RenderViewMenu()
{
	if (ImGui::BeginMenu("보기"))
	{
		// GEditor에서 EditorModule 가져오기
		UEditor* EditorInstance = GEditor->GetEditorModule();
		if (!EditorInstance)
		{
			ImGui::Text("에디터를 사용할 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		EViewModeIndex CurrentMode = EditorInstance->GetViewMode();

		// ViewMode 메뉴 아이템
		bool bIsGroraud = (CurrentMode == EViewModeIndex::VMI_Gouraud);
		bool bIsLambert = (CurrentMode == EViewModeIndex::VMI_Lambert);
		bool bIsBlinnPhong = (CurrentMode == EViewModeIndex::VMI_BlinnPhong);
		bool bIsUnlit = (CurrentMode == EViewModeIndex::VMI_Unlit);
		bool bIsWireframe = (CurrentMode == EViewModeIndex::VMI_Wireframe);
		bool bIsSceneDepth = (CurrentMode == EViewModeIndex::VMI_SceneDepth);

		//if (ImGui::MenuItem("조명 적용(Lit)", nullptr, bIsLit) && !bIsLit)
		//{
		//	EditorInstance->SetViewMode(EViewModeIndex::VMI_Lit);
		//	UE_LOG("MainBarWidget: ViewMode를 Lit으로 변경");
		//}
		if (ImGui::MenuItem("고로 셰이딩 적용(Gouraud)", nullptr, bIsGroraud) && !bIsGroraud)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Gouraud);
			UE_LOG("MainBarWidget: ViewMode를 고로 셰이딩으로 변경");
		}
		if (ImGui::MenuItem("램버트 셰이딩 적용(Lambert)", nullptr, bIsLambert) && !bIsLambert)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Lambert);
			UE_LOG("MainBarWidget: ViewMode를 램버트 셰이딩로 변경");
		}
		if (ImGui::MenuItem("블린-퐁 셰이딩 적용(Blinn-Phong)", nullptr, bIsBlinnPhong) && !bIsBlinnPhong)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_BlinnPhong);
			UE_LOG("MainBarWidget: ViewMode를 블린-퐁 셰이딩으로 변경");
		}
		if (ImGui::MenuItem("조명 비적용(Unlit)", nullptr, bIsUnlit) && !bIsUnlit)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Unlit);
			UE_LOG("MainBarWidget: ViewMode를 Unlit으로 변경");
		}

		if (ImGui::MenuItem("와이어프레임(Wireframe)", nullptr, bIsWireframe) && !bIsWireframe)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_Wireframe);
			UE_LOG("MainBarWidget: ViewMode를 Wireframe으로 변경");
		}

		if (ImGui::MenuItem("씬 뎁스(SceneDepth)", nullptr, bIsSceneDepth) && !bIsSceneDepth)
		{
			EditorInstance->SetViewMode(EViewModeIndex::VMI_SceneDepth);
			UE_LOG("MainBarWidget: ViewMode를 SceneDepth으로 변경");
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief ShowFlags 메뉴를 렌더링하는 함수
 * Static Mesh, BillBoard 등의 플래그 상태 확인 및 토글
 */
void UMainBarWidget::RenderShowFlagsMenu()
{
	if (ImGui::BeginMenu("표시 옵션"))
	{
		// 현재 레벨 가져오기
		ULevel* CurrentLevel = GWorld->GetLevel();
		if (!CurrentLevel)
		{
			ImGui::Text("현재 레벨을 찾을 수 없습니다");
			ImGui::EndMenu();
			return;
		}

		// ShowFlags 가져오기
		uint64 ShowFlags = CurrentLevel->GetShowFlags();

		// BillBoard Text 표시 옵션
		bool bShowBillboard = (ShowFlags & EEngineShowFlags::SF_Billboard) != 0;
		if (ImGui::MenuItem("빌보드 표시", nullptr, bShowBillboard))
		{
			if (bShowBillboard)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Billboard);
				UE_LOG("MainBarWidget: 빌보드 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Billboard);
				UE_LOG("MainBarWidget: 빌보드 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Bounds 표시 옵션
		bool bShowBounds = (ShowFlags & EEngineShowFlags::SF_Bounds) != 0;
		if (ImGui::MenuItem("바운딩박스 표시", nullptr, bShowBounds))
		{
			if (bShowBounds)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Bounds);
				UE_LOG("MainBarWidget: 바운딩박스 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Bounds);
				UE_LOG("MainBarWidget: 바운딩박스 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// StaticMesh 표시 옵션
		bool bShowStaticMesh = (ShowFlags & EEngineShowFlags::SF_StaticMesh) != 0;
		if (ImGui::MenuItem("스태틱 메쉬 표시", nullptr, bShowStaticMesh))
		{
			if (bShowStaticMesh)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
				UE_LOG("MainBarWidget: 스태틱 메쉬 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_StaticMesh);
				UE_LOG("MainBarWidget: 스태틱 메쉬 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Text 표시 옵션
		bool bShowText = (ShowFlags & EEngineShowFlags::SF_Text) != 0;
		if (ImGui::MenuItem("텍스트 표시", nullptr, bShowText))
		{
			if (bShowText)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Text);
				UE_LOG("MainBarWidget: 텍스트 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Text);
				UE_LOG("MainBarWidget: 텍스트 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Decal 표시 옵션
		bool bShowDecal = (ShowFlags & EEngineShowFlags::SF_Decal) != 0;
		if (ImGui::MenuItem("데칼 표시", nullptr, bShowDecal))
		{
			if (bShowDecal)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Decal);
				UE_LOG("MainBarWidget: 데칼 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Decal);
				UE_LOG("MainBarWidget: 데칼 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// Octree 표시 옵션
		bool bShowFog = (ShowFlags & EEngineShowFlags::SF_Fog) != 0;
		if (ImGui::MenuItem("Fog 표시", nullptr, bShowFog))
		{
			if (bShowFog)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Fog);
				UE_LOG("MainBarWidget: Fog 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Fog);
				UE_LOG("MainBarWidget: Fog 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}
		
		// Octree 표시 옵션
		bool bShowOctree = (ShowFlags & EEngineShowFlags::SF_Octree) != 0;
		if (ImGui::MenuItem("Octree 표시", nullptr, bShowOctree))
		{
			if (bShowOctree)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_Octree);
				UE_LOG("MainBarWidget: Octree 비표시");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_Octree);
				UE_LOG("MainBarWidget: Octree 표시");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		// FXAA 표시 옵션
		bool bEnableFXAA = (ShowFlags & EEngineShowFlags::SF_FXAA) != 0;
		if (ImGui::MenuItem("FXAA 적용", nullptr, bEnableFXAA))
		{
			if (bEnableFXAA)
			{
				ShowFlags &= ~static_cast<uint64>(EEngineShowFlags::SF_FXAA);
				UE_LOG("MainBarWidget: FXAA 비활성화");
			}
			else
			{
				ShowFlags |= static_cast<uint64>(EEngineShowFlags::SF_FXAA);
				UE_LOG("MainBarWidget: FXAA 활성화");
			}
			CurrentLevel->SetShowFlags(ShowFlags);
		}

		ImGui::EndMenu();
	}
}

/**
 * @brief Help 메뉴에 대한 렌더링 함수
 */
void UMainBarWidget::RenderHelpMenu()
{
	if (ImGui::BeginMenu("도움말"))
	{
		if (ImGui::MenuItem("정보", "F1"))
		{
			UE_LOG("MainBarWidget: 정보 메뉴 선택됨");
			// TODO(KHJ): 정보 다이얼로그 표시
		}

		ImGui::EndMenu();
	}
}



