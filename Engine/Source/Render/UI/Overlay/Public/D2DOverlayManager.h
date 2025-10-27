#pragma once

class UCamera;

/**
 * @brief D2D 오버레이 렌더링 명령을 수집하고 배치 렌더링하는 매니저
 * FAxis, StatOverlay, Gizmo 각도 등 모든 2D 오버레이를 한 번의 BeginDraw/EndDraw로 처리
 */
class FD2DOverlayManager
{
public:
	static FD2DOverlayManager& GetInstance()
	{
		static FD2DOverlayManager Instance;
		return Instance;
	}

	/**
	 * @brief 렌더링 명령 수집 시작
	 */
	void BeginCollect(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);

	/**
	 * @brief 라인 렌더링 명령 추가
	 */
	void AddLine(const D2D1_POINT_2F& Start, const D2D1_POINT_2F& End, const D2D1_COLOR_F& Color, float Thickness = 1.0f);

	/**
	 * @brief 원 렌더링 명령 추가
	 */
	void AddEllipse(const D2D1_POINT_2F& Center, float RadiusX, float RadiusY, const D2D1_COLOR_F& Color, bool bFilled = true);

	/**
	 * @brief 사각형 렌더링 명령 추가
	 */
	void AddRectangle(const D2D1_RECT_F& Rect, const D2D1_COLOR_F& Color, bool bFilled = true);

	/**
	 * @brief 텍스트 렌더링 명령 추가
	 */
	void AddText(const wchar_t* Text, const D2D1_RECT_F& Rect, const D2D1_COLOR_F& Color, float FontSize = 13.0f, bool bBold = true, bool bCentered = true, const wchar_t* FontName = L"Arial");

	/**
	 * @brief 수집된 모든 렌더링 명령을 실행하고 클리어
	 */
	void FlushAndRender();

private:
	FD2DOverlayManager() = default;
	~FD2DOverlayManager() = default;
	FD2DOverlayManager(const FD2DOverlayManager&) = delete;
	FD2DOverlayManager& operator=(const FD2DOverlayManager&) = delete;

	struct FLineCommand
	{
		D2D1_POINT_2F Start;
		D2D1_POINT_2F End;
		D2D1_COLOR_F Color;
		float Thickness;
	};

	struct FEllipseCommand
	{
		D2D1_POINT_2F Center;
		float RadiusX;
		float RadiusY;
		D2D1_COLOR_F Color;
		bool bFilled;
	};

	struct FRectangleCommand
	{
		D2D1_RECT_F Rect;
		D2D1_COLOR_F Color;
		bool bFilled;
	};

	struct FTextCommand
	{
		std::wstring Text;
		D2D1_RECT_F Rect;
		D2D1_COLOR_F Color;
		float FontSize;
		bool bBold;
		bool bCentered;
		std::wstring FontName;
	};

	std::vector<FLineCommand> LineCommands;
	std::vector<FEllipseCommand> EllipseCommands;
	std::vector<FRectangleCommand> RectangleCommands;
	std::vector<FTextCommand> TextCommands;

	UCamera* CurrentCamera = nullptr;
	D3D11_VIEWPORT CurrentViewport = {};
};
