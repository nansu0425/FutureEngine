#include "pch.h"
#include "Render/Renderer/Public/D2DOverlayManager.h"
#include "Render/Renderer/Public/Renderer.h"

void FD2DOverlayManager::BeginCollect(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	CurrentCamera = InCamera;
	CurrentViewport = InViewport;

	LineCommands.clear();
	EllipseCommands.clear();
	TextCommands.clear();
}

void FD2DOverlayManager::AddLine(const D2D1_POINT_2F& Start, const D2D1_POINT_2F& End, const D2D1_COLOR_F& Color, float Thickness)
{
	FLineCommand Cmd;
	Cmd.Start = Start;
	Cmd.End = End;
	Cmd.Color = Color;
	Cmd.Thickness = Thickness;
	LineCommands.push_back(Cmd);
}

void FD2DOverlayManager::AddEllipse(const D2D1_POINT_2F& Center, float RadiusX, float RadiusY, const D2D1_COLOR_F& Color, bool bFilled)
{
	FEllipseCommand Cmd;
	Cmd.Center = Center;
	Cmd.RadiusX = RadiusX;
	Cmd.RadiusY = RadiusY;
	Cmd.Color = Color;
	Cmd.bFilled = bFilled;
	EllipseCommands.push_back(Cmd);
}

void FD2DOverlayManager::AddText(const wchar_t* Text, const D2D1_RECT_F& Rect, const D2D1_COLOR_F& Color, float FontSize, bool bBold, bool bCentered, const wchar_t* FontName)
{
	FTextCommand Cmd;
	Cmd.Text = Text;
	Cmd.Rect = Rect;
	Cmd.Color = Color;
	Cmd.FontSize = FontSize;
	Cmd.bBold = bBold;
	Cmd.bCentered = bCentered;
	Cmd.FontName = FontName;
	TextCommands.push_back(Cmd);
}

void FD2DOverlayManager::FlushAndRender()
{
	ID2D1RenderTarget* D2DRT = URenderer::GetInstance().GetDeviceResources()->GetD2DRenderTarget();
	if (!D2DRT)
	{
		return;
	}

	IDWriteFactory* DWriteFactory = URenderer::GetInstance().GetDeviceResources()->GetDWriteFactory();

	// D2D 렌더링 시작
	D2DRT->BeginDraw();

	// 모든 라인 그리기
	for (const FLineCommand& Cmd : LineCommands)
	{
		ID2D1SolidColorBrush* Brush = nullptr;
		D2DRT->CreateSolidColorBrush(Cmd.Color, &Brush);
		if (Brush)
		{
			D2DRT->DrawLine(Cmd.Start, Cmd.End, Brush, Cmd.Thickness);
			Brush->Release();
		}
	}

	// 모든 원 그리기
	for (const FEllipseCommand& Cmd : EllipseCommands)
	{
		ID2D1SolidColorBrush* Brush = nullptr;
		D2DRT->CreateSolidColorBrush(Cmd.Color, &Brush);
		if (Brush)
		{
			D2D1_ELLIPSE Ellipse = D2D1::Ellipse(Cmd.Center, Cmd.RadiusX, Cmd.RadiusY);
			if (Cmd.bFilled)
			{
				D2DRT->FillEllipse(Ellipse, Brush);
			}
			else
			{
				D2DRT->DrawEllipse(Ellipse, Brush);
			}
			Brush->Release();
		}
	}

	// 모든 텍스트 그리기
	if (DWriteFactory && !TextCommands.empty())
	{
		for (const FTextCommand& Cmd : TextCommands)
		{
			IDWriteTextFormat* TextFormat = nullptr;
			DWriteFactory->CreateTextFormat(
				Cmd.FontName.c_str(),
				nullptr,
				Cmd.bBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				Cmd.FontSize,
				L"en-us",
				&TextFormat
			);

			if (TextFormat)
			{
				if (Cmd.bCentered)
				{
					TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
				}
				else
				{
					TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
					TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
				}

				ID2D1SolidColorBrush* Brush = nullptr;
				D2DRT->CreateSolidColorBrush(Cmd.Color, &Brush);
				if (Brush)
				{
					D2DRT->DrawText(Cmd.Text.c_str(), static_cast<UINT32>(Cmd.Text.length()), TextFormat, Cmd.Rect, Brush);
					Brush->Release();
				}

				TextFormat->Release();
			}
		}
	}

	// D2D 렌더링 종료
	D2DRT->EndDraw();

	// 명령 버퍼 클리어
	LineCommands.clear();
	EllipseCommands.clear();
	TextCommands.clear();
}
