#pragma once

#include "RenderPass.h"

struct FPointLightPerFrame
{
    FMatrix InvView;
    FMatrix InvProjection;
    FVector CameraLocation;
    float Padding;
};

struct FPointLightData
{
    FVector LightLocation;
    float LightIntensity;
    FVector LightColor;
    float LightRadius;
    float LightFalloffExtent;
    float Padding[3];
};

class FPointLightPass : public FRenderPass
{
public:
    FPointLightPass(UPipeline* InPipeline,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout,
        ID3D11DepthStencilState* InDS, ID3D11BlendState* InBS);

    void Execute(FRenderingContext& Context) override;
    void Release() override;
    
private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;
    ID3D11BlendState* BS = nullptr;

    ID3D11Buffer* VertexBuffer;
    ID3D11Buffer* ConstantBufferPerFrame = nullptr;
    ID3D11Buffer* ConstantBufferPointLightData = nullptr;

    ID3D11SamplerState* PointLightSampler;
};
