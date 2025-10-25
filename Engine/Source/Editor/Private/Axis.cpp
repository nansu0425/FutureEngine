#include "pch.h"
#include "Editor/Public/Axis.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/Renderer.h"
#include <d2d1.h>

FAxis::FAxis() = default;

FAxis::~FAxis() = default;

void FAxis::Render(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
    if (!InCamera)
    {
	    return;
    }

    // D2D 리소스 가져오기
    ID2D1RenderTarget* D2DRT = URenderer::GetInstance().GetDeviceResources()->GetD2DRenderTarget();

    if (!D2DRT)
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

	// D2D 브러시 생성
	ID2D1SolidColorBrush* BrushX = nullptr;
	ID2D1SolidColorBrush* BrushY = nullptr;
	ID2D1SolidColorBrush* BrushZ = nullptr;
	ID2D1SolidColorBrush* BrushCenter = nullptr;

	D2DRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.0f, 0.0f), &BrushX); // 빨강
	D2DRT->CreateSolidColorBrush(D2D1::ColorF(0.0f, 1.0f, 0.0f), &BrushY); // 초록
	D2DRT->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.4f, 1.0f), &BrushZ); // 파랑
	D2DRT->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.2f, 0.2f), &BrushCenter); // 회색

	// D2D 렌더링 시작
	D2DRT->BeginDraw();

	// X, Y, Z 축 라인 그리기
	D2DRT->DrawLine(AxisCenter, AxisEndX, BrushX, LineThick);
	D2DRT->DrawLine(AxisCenter, AxisEndY, BrushY, LineThick);
	D2DRT->DrawLine(AxisCenter, AxisEndZ, BrushZ, LineThick);

	// 중심점 그리기
	D2D1_ELLIPSE OuterCircle = D2D1::Ellipse(AxisCenter, 3.0f, 3.0f);
	D2D1_ELLIPSE InnerCircle = D2D1::Ellipse(AxisCenter, 1.5f, 1.5f);
	D2DRT->FillEllipse(OuterCircle, BrushCenter);
	D2DRT->FillEllipse(InnerCircle, BrushCenter);

	D2DRT->EndDraw();

	// 브러시 해제
	if (BrushX) BrushX->Release();
	if (BrushY) BrushY->Release();
	if (BrushZ) BrushZ->Release();
	if (BrushCenter) BrushCenter->Release();
}
