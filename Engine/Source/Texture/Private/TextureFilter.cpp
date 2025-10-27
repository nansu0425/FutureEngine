#include "pch.h"

#include <filesystem>

#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Texture/Public/TextureFilter.h"

FTextureFilter::FTextureFilter(const FString& InShaderPath)
{
    CreateShader(InShaderPath);
    CreateTexture(DEFAULT_TEXTURE_WIDTH, DEFAULT_TEXTURE_HEIGHT);
}

FTextureFilter::~FTextureFilter() = default;

void FTextureFilter::FilterTexture(ID3D11ShaderResourceView* InTexture, ID3D11UnorderedAccessView* OutTexture,
    uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    ID3D11DeviceContext* DeviceContext = URenderer::GetInstance().GetDeviceContext();
    UPipeline Pipeline(DeviceContext);

    // Get texture description to check the size
    Microsoft::WRL::ComPtr<ID3D11Resource> Resource;
    InTexture->GetResource(Resource.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture2D;
    HRESULT hr = Resource.As(&Texture2D);
    if(FAILED(hr))
	{
		return;
	}

    D3D11_TEXTURE2D_DESC Desc;
    Texture2D->GetDesc(&Desc);

    ResizeTexture(Desc.Width, Desc.Height);

    // --- 1. Row direction filtering ---
    const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (TemporaryUAV) 
    {
        DeviceContext->ClearUnorderedAccessViewFloat(TemporaryUAV.Get(), ClearColor);
    }
    
    Pipeline.SetShaderResourceView(0, EShaderType::CS, InTexture);
    Pipeline.SetUnorderedAccessView(0, TemporaryUAV.Get());

    Pipeline.DispatchCS(ComputeShaderRow.Get(), ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    Pipeline.SetShaderResourceView(0, EShaderType::CS, nullptr);
    Pipeline.SetUnorderedAccessView(0, nullptr);

    // --- 2. Column direction filtering ---
    if (OutTexture)
    {
        DeviceContext->ClearUnorderedAccessViewFloat(OutTexture, ClearColor);
    }
    
    Pipeline.SetShaderResourceView(0, EShaderType::CS, TemporarySRV.Get());
    Pipeline.SetUnorderedAccessView(0, OutTexture);

    Pipeline.DispatchCS(ComputeShaderColumn.Get(), ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

    Pipeline.SetShaderResourceView(0, EShaderType::CS, nullptr);
    Pipeline.SetUnorderedAccessView(0, nullptr);
}

void FTextureFilter::FilterTexture(ID3D11ShaderResourceView* InTexture, ID3D11UnorderedAccessView* OutTexture,
    uint32 ThreadGroupCount)
{
    ID3D11DeviceContext* DeviceContext = URenderer::GetInstance().GetDeviceContext();
    UPipeline Pipeline(DeviceContext);

    // Get texture description to check the size
    Microsoft::WRL::ComPtr<ID3D11Resource> Resource;
    InTexture->GetResource(Resource.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture2D;
    HRESULT hr = Resource.As(&Texture2D);
    if(FAILED(hr))
    {
        return;
    }

    D3D11_TEXTURE2D_DESC Desc;
    Texture2D->GetDesc(&Desc);

    ResizeTexture(Desc.Width, Desc.Height);

    const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // --- 1. Row direction filtering ---
    if (TemporaryUAV) 
    {
        DeviceContext->ClearUnorderedAccessViewFloat(TemporaryUAV.Get(), ClearColor);
    }
    
    Pipeline.SetShaderResourceView(0, EShaderType::CS, InTexture);
    Pipeline.SetUnorderedAccessView(0, TemporaryUAV.Get());

    Pipeline.DispatchCS(ComputeShaderRow.Get(), 1, ThreadGroupCount, 1);

    Pipeline.SetShaderResourceView(0, EShaderType::CS, nullptr);
    Pipeline.SetUnorderedAccessView(0, nullptr);

    // --- 2. Column direction filtering ---
    if (OutTexture)
    {
        DeviceContext->ClearUnorderedAccessViewFloat(OutTexture, ClearColor);
    }

    Pipeline.SetShaderResourceView(0, EShaderType::CS, TemporarySRV.Get());
    Pipeline.SetUnorderedAccessView(0, OutTexture);

    Pipeline.DispatchCS(ComputeShaderColumn.Get(), ThreadGroupCount, 1, 1);

    Pipeline.SetShaderResourceView(0, EShaderType::CS, nullptr);
    Pipeline.SetUnorderedAccessView(0, nullptr);
}

void FTextureFilter::CreateShader(const FString& InShaderPath)
{
    std::filesystem::path ShaderPath(InShaderPath);

    if (!std::filesystem::exists(ShaderPath))
    {
        throw std::runtime_error("Shader path does not exist: " + ShaderPath.string());
    }

	// 1. Row Scan Shader (Default)
    FRenderResourceFactory::CreateComputeShader(ShaderPath.wstring(), ComputeShaderRow.GetAddressOf(), "mainCS", nullptr);

    // 2. Column Scan Shader (with macro)
    D3D_SHADER_MACRO ColDefines[] = { "SCAN_DIRECTION_COLUMN", "1", nullptr, nullptr };
    FRenderResourceFactory::CreateComputeShader(ShaderPath.wstring(), ComputeShaderColumn.GetAddressOf(), "mainCS", ColDefines);
}

void FTextureFilter::CreateTexture(uint32 InWidth, uint32 InHeight)
{
    ID3D11Device* Device = URenderer::GetInstance().GetDevice();

    // 1. Create temporary texture for buffering
    D3D11_TEXTURE2D_DESC TemporaryTextureDesc = {};
    TemporaryTextureDesc.Width = InWidth;
    TemporaryTextureDesc.Height = InHeight;
    TemporaryTextureDesc.MipLevels = 1;
    TemporaryTextureDesc.ArraySize = 1;
    TemporaryTextureDesc.Format = DXGI_FORMAT_R32G32_TYPELESS;
    TemporaryTextureDesc.SampleDesc.Count = 1;
    TemporaryTextureDesc.SampleDesc.Quality = 0;
    TemporaryTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TemporaryTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    TemporaryTextureDesc.CPUAccessFlags = 0;
    TemporaryTextureDesc.MiscFlags = 0;

    HRESULT hr = Device->CreateTexture2D(&TemporaryTextureDesc, nullptr, TemporaryTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create temporary texture for filtering");
    }

    // 2. Create Unordered Access View (UAV) for temporary buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC TemporaryUAVDesc = {};
    TemporaryUAVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    TemporaryUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    TemporaryUAVDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateUnorderedAccessView(TemporaryTexture.Get(), &TemporaryUAVDesc, TemporaryUAV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create temporary texture UAV");
    }

    // 3. Create Shader Resource View (SRV) for temporary buffer
    D3D11_SHADER_RESOURCE_VIEW_DESC TemporarySRVDesc = {};
    TemporarySRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
    TemporarySRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    TemporarySRVDesc.Texture2D.MostDetailedMip = 0;
    TemporarySRVDesc.Texture2D.MipLevels = 1;

    hr = Device->CreateShaderResourceView(TemporaryTexture.Get(), &TemporarySRVDesc, TemporarySRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create temporary texture SRV");
    }
}

void FTextureFilter::ResizeTexture(uint32 InWidth, uint32 InHeight)
{
    if (!TemporaryTexture)
    {
        CreateTexture(InWidth, InHeight);
        return;
    }

    D3D11_TEXTURE2D_DESC Desc;
    TemporaryTexture->GetDesc(&Desc);

    if (Desc.Width != InWidth || Desc.Height != InHeight)
    {
        TemporaryTexture.Reset();
        TemporarySRV.Reset();
        TemporaryUAV.Reset();
        CreateTexture(InWidth, InHeight);
    }
}