#include "pch.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/Camera.h"
#include "Render/RenderPass/Public/PointLightPass.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FPointLightPass::FPointLightPass(UPipeline* InPipeline,
    ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout,
    ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS)
        : FRenderPass(InPipeline, nullptr, nullptr), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS), BS(InBS)
{
    FNormalVertex NormalVertices[3] = {};
    NormalVertices[0].Position = { -1.0f, -1.0f, 0.0f };
    NormalVertices[1].Position = { 2.0f, -1.0f, 0.0f };
    NormalVertices[2].Position = { -1.0f, 2.0f, 0.0f };
    
    VertexBuffer = FRenderResourceFactory::CreateVertexBuffer(NormalVertices, sizeof(NormalVertices));
    ConstantBufferPerFrame = FRenderResourceFactory::CreateConstantBuffer<FPointLightPerFrame>();
    ConstantBufferPointLightData = FRenderResourceFactory::CreateConstantBuffer<FPointLightData>();
    PointLightSampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
}

void FPointLightPass::Execute(FRenderingContext& Context)
{
    TIME_PROFILE(PointLightPass)

    auto RS = FRenderResourceFactory::GetRasterizerState( { ECullMode::None, EFillMode::Solid });

    FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, BS };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetVertexBuffer(VertexBuffer, sizeof(FNormalVertex));

    const auto& DeviceResources = URenderer::GetInstance().GetDeviceResources();
    Pipeline->SetTexture(0, false, DeviceResources->GetSceneColorSRV());
    Pipeline->SetTexture(1, false, DeviceResources->GetNormalSRV());
    Pipeline->SetTexture(2, false, DeviceResources->GetDepthSRV());
    Pipeline->SetSamplerState(0, false, PointLightSampler);

    for (auto PointLight : Context.PointLights)
    {
        // if (!PointLight || !PointLight->IsVisible()) { continue; }
        if (!PointLight) { continue; }

        auto ViewProjConstantsInverse = Context.CurrentCamera->GetFViewProjConstantsInverse();
        
        FPointLightPerFrame PointLightPerFrame = {};
        PointLightPerFrame.InvView = ViewProjConstantsInverse.View;;
        PointLightPerFrame.InvProjection = ViewProjConstantsInverse.Projection;
        PointLightPerFrame.CameraLocation = Context.CurrentCamera->GetLocation();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPerFrame, PointLightPerFrame);
        Pipeline->SetConstantBuffer(0, false, ConstantBufferPerFrame);
        
        FPointLightData PointLightData = {};
        PointLightData.LightLocation = PointLight->GetWorldLocation();
        PointLightData.LightIntensity = PointLight->GetIntensity();
        PointLightData.LightColor = PointLight->GetLightColor();
        PointLightData.LightRadius = PointLight->GetSourceRadius();
        PointLightData.LightFalloffExtent = PointLight->GetLightFalloffExtent();

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPointLightData, PointLightData);
        Pipeline->SetConstantBuffer(1, false, ConstantBufferPointLightData);

        Pipeline->Draw(3, 0);
    }
}

void FPointLightPass::Release()
{
    SafeRelease(VertexBuffer);
    SafeRelease(ConstantBufferPerFrame);
    SafeRelease(ConstantBufferPointLightData);
    SafeRelease(PointLightSampler);
}