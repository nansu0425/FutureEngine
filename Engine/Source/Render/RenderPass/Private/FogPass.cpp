#include "pch.h"
#include "Render/RenderPass/Public/FogPass.h"

#include "Component/Public/HeightFogComponent.h"
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
        
    }
}

void FFogPass::Release()
{
    SafeRelease(ConstantBufferFog);
    SafeRelease(ConstantBufferCameraInverse);
}
