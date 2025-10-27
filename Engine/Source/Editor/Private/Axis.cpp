#include "pch.h"
#include "Editor/Public/Axis.h"
#include "Render/Renderer/Public/D2DOverlayManager.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/Renderer.h"
#include <d2d1.h>

FAxis::FAxis() = default;

FAxis::~FAxis() = default;

void FAxis::CollectDrawCommands(FD2DOverlayManager& Manager, UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
    if (!InCamera)
    {
	    return;
    }

    // 뷰포트 기준 2D 렌더링 원점 계산
    // 뷰포트 하단 왼쪽 모서리로부터 (OffsetFromLeft, OffsetFromBottom) 픽셀 떨어진 위치
    const float OriginX = InViewport.TopLeftX + OffsetFromLeft;
    const float OriginY = InViewport.TopLeftY + InViewport.Height - OffsetFromBottom;
    D2D1_POINT_2F AxisCenter = D2D1::Point2F(OriginX, OriginY);

	// 메인 카메라의 회전(View) 행렬
	const FMatrix ViewOnly = InCamera->GetFViewProjConstants().View;

	// 월드 축 변환
	// 월드 고정 축 (1,0,0), (0,1,0), (0,0,1)을 View 공간으로 변환
	FVector4 WorldX(1, 0, 0, 0);
	FVector4 WorldY(0, 1, 0, 0);
	FVector4 WorldZ(0, 0, 1, 0);

	FVector4 ViewAxisX(
		WorldX.X * ViewOnly.Data[0][0] + WorldX.Y * ViewOnly.Data[1][0] + WorldX.Z * ViewOnly.Data[2][0],
		WorldX.X * ViewOnly.Data[0][1] + WorldX.Y * ViewOnly.Data[1][1] + WorldX.Z * ViewOnly.Data[2][1],
		WorldX.X * ViewOnly.Data[0][2] + WorldX.Y * ViewOnly.Data[1][2] + WorldX.Z * ViewOnly.Data[2][2],
		0.f
	);

	FVector4 ViewAxisY(
		WorldY.X * ViewOnly.Data[0][0] + WorldY.Y * ViewOnly.Data[1][0] + WorldY.Z * ViewOnly.Data[2][0],
		WorldY.X * ViewOnly.Data[0][1] + WorldY.Y * ViewOnly.Data[1][1] + WorldY.Z * ViewOnly.Data[2][1],
		WorldY.X * ViewOnly.Data[0][2] + WorldY.Y * ViewOnly.Data[1][2] + WorldY.Z * ViewOnly.Data[2][2],
		0.f
	);

	FVector4 ViewAxisZ(
		WorldZ.X * ViewOnly.Data[0][0] + WorldZ.Y * ViewOnly.Data[1][0] + WorldZ.Z * ViewOnly.Data[2][0],
		WorldZ.X * ViewOnly.Data[0][1] + WorldZ.Y * ViewOnly.Data[1][1] + WorldZ.Z * ViewOnly.Data[2][1],
		WorldZ.X * ViewOnly.Data[0][2] + WorldZ.Y * ViewOnly.Data[1][2] + WorldZ.Z * ViewOnly.Data[2][2],
		0.f
	);

	// View 공간을 2D 화면으로 매핑
	// View.X (카메라 기준 오른쪽) → Screen X
	// View.Y (카메라 기준 위) → Screen -Y (화면 Y축 반전)
	auto ViewToScreen = [&](const FVector4& ViewVec) -> D2D1_POINT_2F
	{
		float ScreenX = ViewVec.X * AxisSize;
		float ScreenY = -ViewVec.Y * AxisSize;

		return D2D1::Point2F(AxisCenter.x + ScreenX, AxisCenter.y + ScreenY);
	};

	// 최종 2D 좌표 계산
	D2D1_POINT_2F AxisEndX = ViewToScreen(ViewAxisX);
	D2D1_POINT_2F AxisEndY = ViewToScreen(ViewAxisY);
	D2D1_POINT_2F AxisEndZ = ViewToScreen(ViewAxisZ);

	// 텍스트 위치 계산
	constexpr float TextOffset = 1.25f;
	auto ViewToScreenWithOffset = [&](const FVector4& ViewVec, float Offset) -> D2D1_POINT_2F
	{
		float ScreenX = ViewVec.X * AxisSize * Offset;
		float ScreenY = -ViewVec.Y * AxisSize * Offset;
		return D2D1::Point2F(AxisCenter.x + ScreenX, AxisCenter.y + ScreenY);
	};

	D2D1_POINT_2F TextPosX = ViewToScreenWithOffset(ViewAxisX, TextOffset);
	D2D1_POINT_2F TextPosY = ViewToScreenWithOffset(ViewAxisY, TextOffset);
	D2D1_POINT_2F TextPosZ = ViewToScreenWithOffset(ViewAxisZ, TextOffset);

	// 색상 정의
	const D2D1_COLOR_F ColorX = D2D1::ColorF(1.0f, 0.0f, 0.0f); // 빨강
	const D2D1_COLOR_F ColorY = D2D1::ColorF(0.0f, 1.0f, 0.0f); // 초록
	const D2D1_COLOR_F ColorZ = D2D1::ColorF(0.0f, 0.4f, 1.0f); // 파랑
	const D2D1_COLOR_F ColorCenter = D2D1::ColorF(0.2f, 0.2f, 0.2f); // 회색

	// X, Y, Z 축 라인 그리기
	Manager.AddLine(AxisCenter, AxisEndX, ColorX, LineThick);
	Manager.AddLine(AxisCenter, AxisEndY, ColorY, LineThick);
	Manager.AddLine(AxisCenter, AxisEndZ, ColorZ, LineThick);

	// 중심점 그리기
	Manager.AddEllipse(AxisCenter, 3.0f, 3.0f, ColorCenter, true);
	Manager.AddEllipse(AxisCenter, 1.5f, 1.5f, ColorCenter, true);

	// 축 레이블 텍스트 그리기
	constexpr float TextBoxSize = 16.0f;

	// Ortho 뷰에서만 카메라를 정면으로 향하는 축 텍스트 숨김
	const bool bIsOrtho = (InCamera->GetCameraType() == ECameraType::ECT_Orthographic);
	constexpr float VisibilityThreshold = 0.98f;  // 거의 완전히 수직일 때만 숨김

	// X축 텍스트
	bool bShowX = true;
	if (bIsOrtho && std::abs(ViewAxisX.Z) >= VisibilityThreshold)
	{
		bShowX = false;
	}
	if (bShowX)
	{
		D2D1_RECT_F RectX = D2D1::RectF(
			TextPosX.x - TextBoxSize,
			TextPosX.y - TextBoxSize,
			TextPosX.x + TextBoxSize,
			TextPosX.y + TextBoxSize
		);
		Manager.AddText(L"X", RectX, ColorX, 13.0f, true, true, L"Consolas");
	}

	// Y축 텍스트
	bool bShowY = true;
	if (bIsOrtho && std::abs(ViewAxisY.Z) >= VisibilityThreshold)
	{
		bShowY = false;
	}
	if (bShowY)
	{
		D2D1_RECT_F RectY = D2D1::RectF(
			TextPosY.x - TextBoxSize,
			TextPosY.y - TextBoxSize,
			TextPosY.x + TextBoxSize,
			TextPosY.y + TextBoxSize
		);
		Manager.AddText(L"Y", RectY, ColorY, 13.0f, true, true, L"Consolas");
	}

	// Z축 텍스트
	bool bShowZ = true;
	if (bIsOrtho && std::abs(ViewAxisZ.Z) >= VisibilityThreshold)
	{
		bShowZ = false;
	}
	if (bShowZ)
	{
		D2D1_RECT_F RectZ = D2D1::RectF(
			TextPosZ.x - TextBoxSize,
			TextPosZ.y - TextBoxSize,
			TextPosZ.x + TextBoxSize,
			TextPosZ.y + TextBoxSize
		);
		Manager.AddText(L"Z", RectZ, ColorZ, 13.0f, true, true, L"Consolas");
	}
}
