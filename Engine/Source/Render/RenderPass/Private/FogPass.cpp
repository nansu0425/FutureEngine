#include "pch.h"
#include "Render/RenderPass/Public/FogPass.h"

#include "Component/Public/HeightFogComponent.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FFogPass::FFogPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj,
                   ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read,
                   ID3D11BlendState* InBlendState)
        : FRenderPass(InPipeline, InConstantBufferViewProj, nullptr),
            VS(InVS), PS(InPS), InputLayout(InLayout), DS_Read(InDS_Read), BlendState(InBlendState)
{
    ConstantBufferFog = FRenderResourceFactory::CreateConstantBuffer<FFogConstants>();
    ConstantBufferCameraInverse = FRenderResourceFactory::CreateConstantBuffer<FCameraInverseConstants>();
    ConstantBufferViewportInfo = FRenderResourceFactory::CreateConstantBuffer<FVIewportConstants>();
}

void FFogPass::Execute(FRenderingContext& Context)
{
    TIME_PROFILE(FogPass)

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Fog)) return;

    // --- Set Pipeline State --- //
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
        DS_Read, PS, BlendState };
    Pipeline->UpdatePipeline(PipelineInfo);

    // --- Draw Fog --- //
    for (UHeightFogComponent* Fog : Context.Fogs)
    {
        FFogConstants FogConstant;
        FogConstant.FogColor = Fog->GetFogInscatteringColor();
        FogConstant.FogDensity = Fog->GetFogDenisity();
        FogConstant.FogHeightFalloff = Fog->GetFogHeightFalloff();
        FogConstant.StartDistance = Fog->GetStartDistance();
        FogConstant.FogCutoffDistance = Fog->GetFogCutoffDistance();
        FogConstant.FogMaxOpacity = Fog->GetFogMaxOpacity();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFog, FogConstant);
        Pipeline->SetConstantBuffer(0, false, ConstantBufferFog);

        FCameraInverseConstants CameraInverseConstants;
        FViewProjConstants ViewProjConstants = Context.CurrentCamera->GetFViewProjConstantsInverse();
        CameraInverseConstants.ProjectionInverse =  ViewProjConstants.Projection;
        CameraInverseConstants.ViewInverse =  ViewProjConstants.View;

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFog, FogConstant);
        Pipeline->SetConstantBuffer(1, false, ConstantBufferFog);

        FVIewportConstants VIewportConstants;
        VIewportConstants.ViewportOffset = {Context.Viewport.TopLeftX, Context.Viewport.TopLeftY};
        VIewportConstants.RenderTargetSize = Context.RenderTargetSize;
        
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFog, FogConstant);
        Pipeline->SetConstantBuffer(2, false, ConstantBufferFog);

        Pipeline->SetTexture(0, false, nullptr);
        Pipeline->SetSamplerState(0, false, nullptr);

        Pipeline->Draw(3,0);
    }
}

void FFogPass::Release()
{
    SafeRelease(ConstantBufferFog);
    SafeRelease(ConstantBufferCameraInverse);
}
