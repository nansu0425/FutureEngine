#include "pch.h"
#include "Render/UI/Widget/Public/StatusBarWidget.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Window/Public/ConsoleWindow.h"

IMPLEMENT_CLASS(UStatusBarWidget, UWidget)

UStatusBarWidget::UStatusBarWidget()
{
	UIManager = &UUIManager::GetInstance();
	if (!UIManager)
	{
		UE_LOG("StatusBarWidget: UIManager를 찾을 수 없습니다!");
		return;
	}

	UE_LOG("StatusBarWidget: 상태바 위젯이 초기화되었습니다");
}

UStatusBarWidget::~UStatusBarWidget()
{
}

void UStatusBarWidget::Initialize()
{
	// 생성자에서 이미 초기화되었으므로 추가 작업 없음
}

void UStatusBarWidget::Update()
{
	// 하단바는 항상 표시되므로 별도 업데이트 로직 없음
}

void UStatusBarWidget::RenderWidget()
{
	if (!bIsStatusBarVisible || !ImGui::GetCurrentContext())
	{
		return;
	}

	const ImVec2 ScreenSize = ImGui::GetIO().DisplaySize;
	const float BottomPadding = 0.0f;

	// 하단바 위치: 화면 하단에 고정 (전체 화면 기준)
	ImGui::SetNextWindowPos(ImVec2(0.0f, ScreenSize.y - StatusBarHeight - BottomPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(ScreenSize.x, StatusBarHeight), ImGuiCond_Always);

	// 하단바 스타일 설정
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

	const ImGuiWindowFlags WindowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	if (ImGui::Begin("##StatusBar", nullptr, WindowFlags))
	{
		// 하단바 높이 갱신 (실제 렌더링된 크기 기준)
		StatusBarHeight = ImGui::GetWindowHeight();

		RenderLeftSection();
		RenderCenterSection();
		RenderRightSection();
	}
	ImGui::End();

	ImGui::PopStyleColor(1);
	ImGui::PopStyleVar(3);
}

void UStatusBarWidget::RenderLeftSection() const
{
	// 왼쪽 영역: Console 버튼
	const float ButtonWidth = 70.0f;

	// Console 버튼 스타일
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));

	if (ImGui::Button("Console", ImVec2(ButtonWidth, 0)))
	{
		UConsoleWindow& Console = UConsoleWindow::GetInstance();
		Console.ToggleConsoleVisibility();
	}

	ImGui::PopStyleColor(3);
}

void UStatusBarWidget::RenderCenterSection() const
{
	// 중앙 영역: 비워둠
}

void UStatusBarWidget::RenderRightSection() const
{
	// 우측 영역: 비워둠
}
