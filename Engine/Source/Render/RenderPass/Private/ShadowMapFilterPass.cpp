#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/RenderPass/Public/ShadowMapPass.h"
#include "Render/RenderPass/Public/ShadowMapFilterPass.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Component/Public/PointLightComponent.h"

FShadowMapFilterPass::FShadowMapFilterPass(FShadowMapPass* InShadowMapPass, UPipeline* InPipeline)
    : FRenderPass(InPipeline), ShadowMapPass(InShadowMapPass)
{
	TextureFilterMap[EShadowModeIndex::SMI_VSM_BOX] = std::make_unique<FTextureFilter>("Asset/Shader/BoxTextureFilter.hlsl");
	TextureFilterMap[EShadowModeIndex::SMI_SAVSM] = std::make_unique<FTextureFilter>("Asset/Shader/SummedAreaTextureFilter.hlsl");
	TextureFilterMap[EShadowModeIndex::SMI_VSM_GAUSSIAN] = std::make_unique<FTextureFilter>("Asset/Shader/GaussianTextureFilter.hlsl");
}

FShadowMapFilterPass::~FShadowMapFilterPass()
{
	// TODO: 소멸자에서 가상 함수 호출하지 말아야 함, 다른 렌더패스 확인 필요
    // Release();
}

void FShadowMapFilterPass::Execute(FRenderingContext& Context)
{
	// --- 1. Directional Lights ---
	for (auto DirLight : Context.DirectionalLights)
	{
		if (DirLight->GetCastShadows() && DirLight->GetLightEnabled())
		{
			FShadowMapResource* ShadowMap = ShadowMapPass->GetDirectionalShadowMap(DirLight);
			FilterShadowMap(DirLight, ShadowMap);
		}
	}

	// --- 2. SpotLights ---
	for (auto SpotLight : Context.SpotLights)
	{
		if (SpotLight->GetCastShadows() && SpotLight->GetLightEnabled())
		{
			FShadowMapResource* ShadowMap = ShadowMapPass->GetSpotShadowMap(SpotLight);
			FilterShadowMap(SpotLight, ShadowMap);
		}
	}

	// --- 3. Point Lights ---
	// TODO
}

void FShadowMapFilterPass::Release()
{
	TextureFilterMap.clear();	
}

void FShadowMapFilterPass::FilterShadowMap(const ULightComponentBase* LightComponent, const FShadowMapResource* ShadowMap) 
{
	if (!ShadowMap || !ShadowMap->IsValid())
	{
		return;
	}

	switch (LightComponent->GetShadowModeIndex())
	{
	case EShadowModeIndex::SMI_VSM_BOX:
		{
			uint32 NumGroupsX = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_X - 1) / THREAD_BLOCK_SIZE_X;
			uint32 NumGroupsY = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_Y - 1) / THREAD_BLOCK_SIZE_Y;
			uint32 NumGroupsZ = 1;
			TextureFilterMap[EShadowModeIndex::SMI_VSM_BOX]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroupsX, NumGroupsY, NumGroupsZ
			);
			break;
		}
	case EShadowModeIndex::SMI_VSM_GAUSSIAN:
		{
			uint32 NumGroupsX = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_X - 1) / THREAD_BLOCK_SIZE_X;
			uint32 NumGroupsY = (ShadowMap->Resolution + THREAD_BLOCK_SIZE_Y - 1) / THREAD_BLOCK_SIZE_Y;
			uint32 NumGroupsZ = 1;
			TextureFilterMap[EShadowModeIndex::SMI_VSM_GAUSSIAN]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroupsX, NumGroupsY, NumGroupsZ
			);
			break;
		}
	case EShadowModeIndex::SMI_SAVSM:
		{
			uint32 NumGroups = ShadowMap->Resolution;
			TextureFilterMap[EShadowModeIndex::SMI_SAVSM]->FilterTexture(
				ShadowMap->VarianceShadowSRV.Get(),
				ShadowMap->VarianceShadowUAV.Get(),
				NumGroups
			);
			break;
		}
	default:
		// 필터링 없음
		break;
	}
}