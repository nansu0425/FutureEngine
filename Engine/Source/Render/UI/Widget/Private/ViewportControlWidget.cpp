#include "pch.h"
#include "Render/UI/Widget/Public/ViewportControlWidget.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"
#include "Editor/Public/Editor.h"

// 정적 멤버 정의 - KTLWeek07의 뷰 모드 기능 사용
const char* UViewportControlWidget::ViewModeLabels[] = {
	"Gouraud", "Lambert", "BlinnPhong", "Unlit", "Wireframe", "SceneDepth"
};

const char* UViewportControlWidget::ViewTypeLabels[] = {
	"Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back"
};

UViewportControlWidget::UViewportControlWidget()
	: UWidget("Viewport Control Widget")
{
}

UViewportControlWidget::~UViewportControlWidget() = default;

void UViewportControlWidget::Initialize()
{
	UE_LOG("ViewportControlWidget: Initialized");
}

void UViewportControlWidget::Update()
{
	// 필요시 업데이트 로직 추가
	
}

void UViewportControlWidget::RenderWidget()
{
	auto& ViewportManager = UViewportManager::GetInstance();
	if (!ViewportManager.GetRoot())
	{
		return;
	}

	// 먼저 스플리터 선 렌더링 (UI 뒤에서)
	//RenderSplitterLines();

	// 싱글 모드에서는 하나만 렌더링
	if (ViewportManager.GetViewportLayout() == EViewportLayout::Single)
	{
		int32 ActiveViewportIndex = ViewportManager.GetActiveIndex();
		RenderViewportToolbar(ActiveViewportIndex);
	}
	else
	{
		for (int32 i = 0;i < 4;++i)
		{
			RenderViewportToolbar(i);
		}
	}
}

void UViewportControlWidget::RenderViewportToolbar(int32 ViewportIndex)
{
	auto& ViewportManager = UViewportManager::GetInstance();
	const auto& Viewports = ViewportManager.GetViewports();
	const auto& Clients = ViewportManager.GetClients();

	// 뷰포트 범위가 벗어날 경우 종료
	if (ViewportIndex >= static_cast<int32>(Viewports.size()) || ViewportIndex >= static_cast<int32>(Clients.size()))
	{
		return;
	}

	// FutureEngine: null 체크 추가
	if (!Viewports[ViewportIndex] || !Clients[ViewportIndex])
	{
		return;
	}

	const FRect& Rect = Viewports[ViewportIndex]->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return;
	}


	const int32 ToolbarH = 32;
	const ImVec2 Vec1{ static_cast<float>(Rect.Left), static_cast<float>(Rect.Top) };
	const ImVec2 Vec2{ static_cast<float>(Rect.Left + Rect.Width), static_cast<float>(Rect.Top + ToolbarH) };

	// 1) 툴바 배경 그리기
	ImDrawList* DrawLine = ImGui::GetBackgroundDrawList();
	DrawLine->AddRectFilled(Vec1, Vec2, IM_COL32(30, 30, 30, 100));
	DrawLine->AddLine(ImVec2(Vec1.x, Vec2.y), ImVec2(Vec2.x, Vec2.y), IM_COL32(70, 70, 70, 120), 1.0f);

	// 2) 툴바 UI 요소들을 직접 배치 (별도 창 생성하지 않음)
	// UI 요소들을 화면 좌표계에서 직접 배치
	ImGui::SetNextWindowPos(ImVec2(Vec1.x, Vec1.y));
	ImGui::SetNextWindowSize(ImVec2(Vec2.x - Vec1.x, Vec2.y - Vec1.y));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoTitleBar;

	// 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 3.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 3.f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 0.f));

	char WinName[64];
	(void)snprintf(WinName, sizeof(WinName), "##ViewportToolbar%d", ViewportIndex);

	ImGui::PushID(ViewportIndex);
	if (ImGui::Begin(WinName, nullptr, flags))
	{
		// ViewMode 콤보박스 - KTLWeek07의 EViewModeIndex 사용
		EViewModeIndex CurrentMode = GEditor->GetEditorModule()->GetViewMode();
		int32 CurrentModeIndex = static_cast<int32>(CurrentMode);

		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::Combo("##ViewMode", &CurrentModeIndex, ViewModeLabels, IM_ARRAYSIZE(ViewModeLabels)))
		{
			if (CurrentModeIndex >= 0 && CurrentModeIndex < IM_ARRAYSIZE(ViewModeLabels))
			{
				GEditor->GetEditorModule()->SetViewMode(static_cast<EViewModeIndex>(CurrentModeIndex));
			}
		}

		// 구분자
		ImGui::SameLine(0.0f, 10.0f);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0.0f, 10.0f);

		// ViewType 콤보박스 - ViewportClient의 EViewType 사용
		EViewType CurType = Clients[ViewportIndex]->GetViewType();
		int32 CurrentIdx = static_cast<int32>(CurType);

		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::Combo("##ViewType", &CurrentIdx, ViewTypeLabels, IM_ARRAYSIZE(ViewTypeLabels)))
		{
			if (CurrentIdx >= 0 && CurrentIdx < IM_ARRAYSIZE(ViewTypeLabels))
			{
				EViewType NewType = static_cast<EViewType>(CurrentIdx);
				Clients[ViewportIndex]->SetViewType(NewType);
				
				// Camera 타입도 동기화
				if (UCamera* Cam = Clients[ViewportIndex]->GetCamera())
				{
					if (NewType == EViewType::Perspective)
						Cam->SetCameraType(ECameraType::ECT_Perspective);
					else
						Cam->SetCameraType(ECameraType::ECT_Orthographic);
				}
			}
		}

		// 구분자
		ImGui::SameLine(0.0f, 10.0f);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0.0f, 10.0f);

		// 카메라 스피드 표시 및 조정
		//RenderCameraSpeedControl(ViewportIndex);

		// 구분자
		ImGui::SameLine(0.0f, 10.0f);
		ImGui::TextDisabled("|");
		ImGui::SameLine(0.0f, 10.0f);

		// 그리드 사이즈 조정
		//RenderGridSizeControl();

		// 레이아웃 전환 버튼들을 우측 정렬
		{
			constexpr float RightPadding = 6.0f;
			constexpr float BetweenWidth = 24.0f;

			const float ContentRight = ImGui::GetWindowContentRegionMax().x;
			float TargetX = ContentRight - RightPadding - BetweenWidth;
			TargetX = std::max(TargetX, ImGui::GetCursorPosX());

			ImGui::SameLine();
			ImGui::SetCursorPosX(TargetX);
		}


		// 레이아웃 전환 버튼들
		if (ViewportManager.GetViewportLayout() == EViewportLayout::Single)
		{
			if (FUEImgui::ToolbarIconButton("LayoutQuad", EUEViewportIcon::Quad, CurrentLayout == ELayout::Quad))
			{
				CurrentLayout = ELayout::Quad;
				ViewportManager.SetViewportLayout(EViewportLayout::Quad);

				ViewportManager.StartLayoutAnimation(true, ViewportIndex);
			}
		}

		if (ViewportManager.GetViewportLayout() == EViewportLayout::Quad)
		{
			if (FUEImgui::ToolbarIconButton("LayoutSingle", EUEViewportIcon::Single, CurrentLayout == ELayout::Single))
			{
				CurrentLayout = ELayout::Single;
				ViewportManager.SetViewportLayout(EViewportLayout::Single);

				// 스플리터 비율을 저장하고 애니메이션 시작: Quad → Single
				ViewportManager.PersistSplitterRatios();
				ViewportManager.StartLayoutAnimation(false, ViewportIndex);
				
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
	ImGui::PopID();
}



