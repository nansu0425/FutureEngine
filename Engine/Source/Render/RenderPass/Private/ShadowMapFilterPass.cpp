#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"
#include "Render/RenderPass/Public/ShadowMapFilterPass.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Component/Public/PointLightComponent.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FShadowMapFilterPass::FShadowMapFilterPass(FShadowMapPass* InShadowMapPass,
                                           UPipeline* InPipeline,
                                           ID3D11ComputeShader* InTextureFilterRowCS,
                                           ID3D11ComputeShader* InTextureFilterColumnCS)
    : FRenderPass(InPipeline)
	, ShadowMapPass(InShadowMapPass)
    , TextureFilterRowCS(InTextureFilterRowCS)
    , TextureFilterColumnCS(InTextureFilterColumnCS)
{
	ID3D11Device* Device = URenderer::GetInstance().GetDevice();
	
	// 1. 임시버퍼용 Texture 생성
	D3D11_TEXTURE2D_DESC TemporaryTextureDesc = {};
	TemporaryTextureDesc.Width = TEXTURE_WIDTH;
	TemporaryTextureDesc.Height = TEXTURE_HEIGHT;
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

	// 2. 임시버퍼용 Unordered Access View(UAV) 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC TemporaryUAVDesc = {};
	TemporaryUAVDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	TemporaryUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	TemporaryUAVDesc.Texture2D.MipSlice = 0;

	hr = Device->CreateUnorderedAccessView(TemporaryTexture.Get(), &TemporaryUAVDesc, TemporaryUAV.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create temporary texture UAV");
	}

	// 3. 임시버퍼용 Shader Resource View(SRV) 생성
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

	// 4. Constant Buffer 생성
	ConstantBufferTextureInfo = FRenderResourceFactory::CreateConstantBuffer<FTextureInfo>();
}

FShadowMapFilterPass::~FShadowMapFilterPass()
{
	// TODO: 소멸자에서 가상 함수 호출하지 말아야 함, 다른 렌더패스 확인 필요
    // Release();
}

void FShadowMapFilterPass::Execute(FRenderingContext& Context)
{
	const auto& Renderer = URenderer::GetInstance();

	// --- 1. Directional Lights ---
	for (auto DirLight : Context.DirectionalLights)
	{
		if (DirLight->GetCastShadows() && DirLight->GetLightEnabled())
		{
			FShadowMapResource* ShadowMap = ShadowMapPass->GetDirectionalShadowMap(DirLight);
			SummedAreaFilterShadowMap(ShadowMap);	
		}
	}

	// --- 2. SpotLights ---
	for (auto SpotLight : Context.SpotLights)
	{
		if (SpotLight->GetCastShadows() && SpotLight->GetLightEnabled())
		{
			FShadowMapResource* ShadowMap = ShadowMapPass->GetSpotShadowMap(SpotLight);
			SummedAreaFilterShadowMap(ShadowMap);	
		}
	}

	// --- 3. Point Lights ---
	// TODO
}

void FShadowMapFilterPass::Release()
{
	ConstantBufferTextureInfo.Reset();
	TemporaryTexture.Reset();
	TemporaryUAV.Reset();
	TemporarySRV.Reset();
}

void FShadowMapFilterPass::SummedAreaFilterShadowMap(const FShadowMapResource* ShadowMap) const
{
	if (!ShadowMap || !ShadowMap->IsValid())
	{
		return;
	}

	const auto& Renderer = URenderer::GetInstance();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

	// 텍스처 정보 업데이트
	FTextureInfo TextureInfo;
	TextureInfo.TextureWidth = ShadowMap->Resolution;
	TextureInfo.TextureHeight = ShadowMap->Resolution;
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferTextureInfo.Get(), TextureInfo);
	Pipeline->SetConstantBuffer(0, EShaderType::CS, ConstantBufferTextureInfo.Get());

	// --- 1. Row 방향 필터링 ---
	Pipeline->SetShaderResourceView(0, EShaderType::CS, ShadowMap->VarianceShadowSRV.Get());
	Pipeline->SetUnorderedAccessView(0, TemporaryUAV.Get());

	Pipeline->DispatchCS(TextureFilterRowCS, 1, ShadowMap->Resolution, 1);

	Pipeline->SetShaderResourceView(0, EShaderType::CS, nullptr);
	Pipeline->SetUnorderedAccessView(0, nullptr);

	// --- 2. Column 방향 필터링 ---
	Pipeline->SetShaderResourceView(0, EShaderType::CS, TemporarySRV.Get());
	Pipeline->SetUnorderedAccessView(0, ShadowMap->SummedAreaVarianceShadowUAV.Get());

	Pipeline->DispatchCS(TextureFilterColumnCS, ShadowMap->Resolution, 1, 1);

	Pipeline->SetShaderResourceView(0, EShaderType::CS, nullptr);
	Pipeline->SetUnorderedAccessView(0, nullptr);
}
