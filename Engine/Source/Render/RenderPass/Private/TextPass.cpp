#include "pch.h"
#include "Render/RenderPass/Public/TextPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/TextComponent.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Camera.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Editor/Public/Editor.h"

FTextPass::FTextPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel)
    : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
{
    // Create shaders
    TArray<D3D11_INPUT_ELEMENT_DESC> layoutDesc = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(UFontRenderer::FFontVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UFontRenderer::FFontVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, offsetof(UFontRenderer::FFontVertex, CharIndex), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/ShaderFont.hlsl", layoutDesc, &FontVertexShader, &FontInputLayout);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/ShaderFont.hlsl", &FontPixelShader);

    // Create sampler state
    FontSampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);

    // Create dynamic vertex buffer
    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.ByteWidth = sizeof(UFontRenderer::FFontVertex) * MAX_FONT_VERTICES;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    URenderer::GetInstance().GetDevice()->CreateBuffer(&BufferDesc, nullptr, &DynamicVertexBuffer);

    // Create constant buffer
    FontDataConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<UFontRenderer::FFontConstantBuffer>();

    // Load font texture
    UAssetManager& ResourceManager = UAssetManager::GetInstance();
    auto TextureComPtr = ResourceManager.LoadTexture("Asset/Texture/DejaVu Sans Mono.png");
    FontAtlasTexture = TextureComPtr.Get();
}

void FTextPass::Execute(FRenderingContext& Context)
{
    // Set up pipeline
    FPipelineInfo PipelineInfo = {};
    PipelineInfo.InputLayout = FontInputLayout;
    PipelineInfo.VertexShader = FontVertexShader;
    PipelineInfo.PixelShader = FontPixelShader;
    PipelineInfo.RasterizerState = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
    PipelineInfo.BlendState = URenderer::GetInstance().GetAlphaBlendState();
    PipelineInfo.DepthStencilState = URenderer::GetInstance().GetDefaultDepthStencilState(); // Or DisabledDepthStencilState based on a flag
    Pipeline->UpdatePipeline(PipelineInfo);
    if (!(Context.ShowFlags & EEngineShowFlags::SF_Text)) { return; }

    // Set constant buffers
    Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
    FRenderResourceFactory::UpdateConstantBufferData(FontDataConstantBuffer, ConstantBufferData);
    Pipeline->SetConstantBuffer(2, true, FontDataConstantBuffer);

    // Bind resources
    Pipeline->SetTexture(0, false, FontAtlasTexture);
    Pipeline->SetSamplerState(0, false, FontSampler);

    for (UTextComponent* Text : Context.Texts)
    {
        RenderTextInternal(Text->GetText(), Text->GetWorldTransformMatrix());
    }

    // Render UUID
    if (!(Context.ShowFlags & EEngineShowFlags::SF_Billboard)) { return; }

    if (UUUIDTextComponent* PickedBillboard = GEditor->GetEditorModule()->GetPickedBillboard())
    {
        PickedBillboard->UpdateRotationMatrix(Context.CurrentCamera->GetLocation());
        FString UUIDString = "UID: " + std::to_string(PickedBillboard->GetUUID());
        RenderTextInternal(UUIDString, PickedBillboard->GetRTMatrix());
    }
}

void FTextPass::RenderTextInternal(const FString& Text, const FMatrix& WorldMatrix)
{
    if (Text.empty()) return;

    ID3D11DeviceContext* DeviceContext = URenderer::GetInstance().GetDeviceContext();

    size_t TextLength = Text.length();
    uint32 VertexCount = static_cast<uint32>(TextLength * 6);

    if (VertexCount > MAX_FONT_VERTICES)
    {
        // Log warning
        return;
    }

    // Update vertex buffer
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(DeviceContext->Map(DynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
    {
        UFontRenderer::FFontVertex* Vertices = static_cast<UFontRenderer::FFontVertex*>(mappedResource.pData);
        float currentY = 0.0f - (TextLength * 1.0f) / 2.0f; // Assuming char width of 1.0f

        for (size_t i = 0; i < TextLength; ++i)
        {
            char ch = Text[i];
            uint32 asciiCode = static_cast<uint32>(ch);

            // Simplified vertex generation, assuming fixed size
            float y = currentY + i * 1.0f;
            float z = -2.5f;
            float charHeight = 2.0f;

            FVector p0(0.0f, y, z + charHeight);
            FVector p1(0.0f, y + 1.0f, z + charHeight);
            FVector p2(0.0f, y, z);
            FVector p3(0.0f, y + 1.0f, z);

            Vertices[i * 6 + 0] = { p0, FVector2(0.0f, 0.0f), asciiCode };
            Vertices[i * 6 + 1] = { p1, FVector2(1.0f, 0.0f), asciiCode };
            Vertices[i * 6 + 2] = { p2, FVector2(0.0f, 1.0f), asciiCode };
            Vertices[i * 6 + 3] = { p1, FVector2(1.0f, 0.0f), asciiCode };
            Vertices[i * 6 + 4] = { p3, FVector2(1.0f, 1.0f), asciiCode };
            Vertices[i * 6 + 5] = { p2, FVector2(0.0f, 1.0f), asciiCode };
        }
        DeviceContext->Unmap(DynamicVertexBuffer, 0);
    }

    // Update model constant buffer
    FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
    Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

    // Set vertex buffer
    Pipeline->SetVertexBuffer(DynamicVertexBuffer, sizeof(UFontRenderer::FFontVertex));

    // Draw
    Pipeline->Draw(VertexCount, 0);
}

void FTextPass::Release()
{
    SafeRelease(FontVertexShader);
    SafeRelease(FontPixelShader);
    SafeRelease(FontInputLayout);
    SafeRelease(FontSampler);
    SafeRelease(DynamicVertexBuffer);
    SafeRelease(FontDataConstantBuffer);
}