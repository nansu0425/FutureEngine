#pragma once
#include "Render/RenderPass/Public/RenderPass.h"
#include "Render/FontRenderer/Public/FontRenderer.h"

class FTextPass : public FRenderPass
{
public:
    FTextPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel);
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    void RenderTextInternal(const FString& Text, const FMatrix& WorldMatrix);
    
    // Font rendering resources
    ID3D11VertexShader* FontVertexShader = nullptr;
    ID3D11PixelShader* FontPixelShader = nullptr;
    ID3D11InputLayout* FontInputLayout = nullptr;
    ID3D11SamplerState* FontSampler = nullptr;
    ID3D11ShaderResourceView* FontAtlasTexture = nullptr;
    ID3D11Buffer* DynamicVertexBuffer = nullptr;
    ID3D11Buffer* FontDataConstantBuffer = nullptr;
    UFontRenderer::FFontConstantBuffer ConstantBufferData;

    static constexpr uint32 MAX_FONT_VERTICES = 4096;
};