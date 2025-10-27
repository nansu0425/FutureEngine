#pragma once

class UCamera;
class FD2DOverlayManager;
struct ImDrawList;

/**
 * @brief 뷰포트 좌하단에 표시되는 카메라 방향 축 위젯
 * D2DOverlayManager에 렌더링 명령 추가
 */
class FAxis
{
public:
	FAxis();
	virtual	~FAxis();

	static void CollectDrawCommands(FD2DOverlayManager& Manager, UCamera* InCamera, const D3D11_VIEWPORT& InViewport);

private:
	// 뷰포트 좌하단 오프셋
	static constexpr float OffsetFromLeft = 70.0f;
	static constexpr float OffsetFromBottom = 70.0f;
	static constexpr float AxisSize = 40.0f;

	// 라인 두께
	static constexpr float LineThick = 3.0f;
};
