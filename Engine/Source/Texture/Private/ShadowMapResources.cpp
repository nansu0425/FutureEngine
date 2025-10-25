#include "pch.h"
#include "Texture/Public/ShadowMapResources.h"
#include <stdexcept>

// =============================================================================
// FShadowMapResource Implementation
// =============================================================================

void FShadowMapResource::Initialize(ID3D11Device* Device, uint32 InResolution)
{
	// 기존 리소스 해제
	Release();
	Resolution = InResolution;

	// 1. Depth Texture 생성
	// - R32_TYPELESS: Depth와 SRV 둘 다 지원하는 포맷
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = Resolution;
	TexDesc.Height = Resolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;  // Depth + SRV 지원
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	TexDesc.CPUAccessFlags = 0;
	TexDesc.MiscFlags = 0;

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, ShadowTexture.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create shadow map texture");
	}

	// 2. Depth Stencil View (DSV) 생성
	// - D32_FLOAT: Depth 쓰기용
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateDepthStencilView(ShadowTexture.Get(), &DSVDesc, ShadowDSV.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create shadow DSV");
	}

	// 3. Shader Resource View (SRV) 생성
	// - R32_FLOAT: Shader에서 depth sampling용
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	hr = Device->CreateShaderResourceView(ShadowTexture.Get(), &SRVDesc, ShadowSRV.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create shadow SRV");
	}

	// 4. VSM용 Depth Texture 생성
	D3D11_TEXTURE2D_DESC VarianceTexDesc = {};
	VarianceTexDesc.Width = Resolution;
	VarianceTexDesc.Height = Resolution;
	VarianceTexDesc.MipLevels = 1;
	VarianceTexDesc.ArraySize = 1;
	VarianceTexDesc.Format = DXGI_FORMAT_R32G32_TYPELESS; // R32 -> 1차 모멘트, G32 -> 2차 모멘트
	VarianceTexDesc.SampleDesc.Count = 1;
	VarianceTexDesc.SampleDesc.Quality = 0;
	VarianceTexDesc.Usage = D3D11_USAGE_DEFAULT;
	VarianceTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	VarianceTexDesc.CPUAccessFlags = 0;
	VarianceTexDesc.MiscFlags = 0;

	hr = Device->CreateTexture2D(&VarianceTexDesc, nullptr, VarianceShadowTexture.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create VSM texture");
	}

	// 5. VSM용 Render Target View(RTV) 생성
	D3D11_RENDER_TARGET_VIEW_DESC VarianceRTVDesc = {};
	VarianceRTVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	VarianceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	VarianceRTVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateRenderTargetView(VarianceShadowTexture.Get(), &VarianceRTVDesc, VarianceShadowRTV.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create VSM RTV");
	}

	// 6. VSM용 Shader Resource View(SRV) 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC VarianceSRVDesc = {};
	VarianceSRVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	VarianceSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	VarianceSRVDesc.Texture2D.MostDetailedMip = 0;
	VarianceSRVDesc.Texture2D.MipLevels = 1;

	hr = Device->CreateShaderResourceView(VarianceShadowTexture.Get(), &VarianceSRVDesc, VarianceShadowSRV.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create VSM SRV");
	}

	// 7. Viewport 설정
	ShadowViewport.Width = static_cast<float>(Resolution);
	ShadowViewport.Height = static_cast<float>(Resolution);
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
}

void FShadowMapResource::Release()
{
	// ComPtr을 사용하므로 Reset()만 호출하면 자동으로 해제됨
	ShadowTexture.Reset();
	ShadowDSV.Reset();
	ShadowSRV.Reset();
	VarianceShadowTexture.Reset();
	VarianceShadowRTV.Reset();
	VarianceShadowSRV.Reset();
}

// =============================================================================
// FCubeShadowMapResource Implementation
// =============================================================================

void FCubeShadowMapResource::Initialize(ID3D11Device* Device, uint32 InResolution)
{
	// 기존 리소스 해제
	Release();
	Resolution = InResolution;

	// 1. Cube Depth Texture 생성
	// - ArraySize = 6: Cube의 6면
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = Resolution;
	TexDesc.Height = Resolution;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 6;  // Cube의 6면
	TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	TexDesc.CPUAccessFlags = 0;
	TexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;  // Cube texture flag

	HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, ShadowTextureCube.GetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create cube shadow map texture");
	}

	// 2. 각 면에 대한 DSV 생성
	// +X, -X, +Y, -Y, +Z, -Z 순서
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
	DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	DSVDesc.Texture2DArray.MipSlice = 0;
	DSVDesc.Texture2DArray.ArraySize = 1;  // 한 번에 한 면씩

	for (uint32 i = 0; i < 6; i++)
	{
		DSVDesc.Texture2DArray.FirstArraySlice = i;
		hr = Device->CreateDepthStencilView(ShadowTextureCube.Get(), &DSVDesc, ShadowDSVs[i].GetAddressOf());
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create cube shadow DSV");
		}
	}

	// 3. Cube SRV 생성
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	SRVDesc.TextureCube.MipLevels = 1;

	hr = Device->CreateShaderResourceView(ShadowTextureCube.Get(), &SRVDesc, ShadowSRV.GetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create cube shadow SRV");
	}

	// 4. Viewport 설정
	ShadowViewport.Width = static_cast<float>(Resolution);
	ShadowViewport.Height = static_cast<float>(Resolution);
	ShadowViewport.MinDepth = 0.0f;
	ShadowViewport.MaxDepth = 1.0f;
	ShadowViewport.TopLeftX = 0.0f;
	ShadowViewport.TopLeftY = 0.0f;
}

void FCubeShadowMapResource::Release()
{
	// ComPtr을 사용하므로 Reset()만 호출
	ShadowTextureCube.Reset();

	for (int i = 0; i < 6; i++)
	{
		ShadowDSVs[i].Reset();
	}

	ShadowSRV.Reset();
}
