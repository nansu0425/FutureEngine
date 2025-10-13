#pragma once
#include "Render/RenderPass/Public/RenderPass.h"


class UDeviceResources;

struct FFXAAConstants
{
    FVector2 InvResolution;
    FVector2 Padding;
};


class FFXAAPass : public FRenderPass
{
public:
    FFXAAPass(UPipeline* InPipeline,
      UDeviceResources* InDeviceResources,
      ID3D11VertexShader* InVS,
      ID3D11PixelShader* InPS,
      ID3D11InputLayout* InLayout,
      ID3D11SamplerState* InSampler);
    void Execute(FRenderingContext& Context) override;
    void Release() override;


private:
    void InitializeFullscreenQuad();
    void UpdateConstants();
    void SetRenderTargets();
    
private:
    UDeviceResources* DeviceResources = nullptr;

    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11SamplerState* SamplerState = nullptr;

    ID3D11Buffer* FullscreenVB = nullptr;
    ID3D11Buffer* FullscreenIB = nullptr;
    UINT FullscreenStride = 0;
    UINT FullscreenIndexCount = 0;

    ID3D11Buffer* FXAAConstantBuffer = nullptr;
    FFXAAConstants FXAAParams{};
};
