#pragma once
#include "Render/UI/Widget/Public/Widget.h"

class UUIManager;

/**
 * @brief ImGui 하단 상태바를 관리하는 위젯 클래스
 * 화면 하단에 고정되어 FPS, 시간, 상태 정보 등을 표시
 */
UCLASS()
class UStatusBarWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UStatusBarWidget, UWidget)

public:
	UStatusBarWidget();
	~UStatusBarWidget() override;

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	float GetStatusBarHeight() const { return StatusBarHeight; }
	bool IsStatusBarVisible() const { return bIsStatusBarVisible; }

private:
	bool bIsStatusBarVisible = true;
	float StatusBarHeight = 25.0f; // 하단바 기본 높이
	UUIManager* UIManager = nullptr;

	// 상태바 렌더링 함수들
	void RenderLeftSection() const;
	void RenderCenterSection() const;
	void RenderRightSection() const;
};
