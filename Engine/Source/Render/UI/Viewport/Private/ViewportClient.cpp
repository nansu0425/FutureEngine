#include "pch.h"

#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"

FViewportClient::FViewportClient()
{
    ViewportCamera = NewObject<UCamera>();
}

FViewportClient::~FViewportClient()
{
    SafeDelete(ViewportCamera);
}

void FViewportClient::Tick() const
{
    // FutureEngine 철학: Camera는 Viewport의 크기 정보를 받아야 함
    if (OwningViewport && ViewportCamera)
    {
        const D3D11_VIEWPORT ViewportRect = OwningViewport->GetRenderRect();
        ViewportCamera->Update(ViewportRect);
    }
}

void FViewportClient::Draw(const FViewport* InViewport) const
{
    if (!InViewport) { return; }

    const float Aspect = InViewport->GetAspect();
    ViewportCamera->SetAspect(Aspect);

    if (IsOrtho())
    {
		
        ViewportCamera->UpdateMatrixByOrth();
		
    }
    else
    {
        ViewportCamera->UpdateMatrixByPers();
    }
}
