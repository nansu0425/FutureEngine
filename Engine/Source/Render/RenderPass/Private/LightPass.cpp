#include "pch.h"
#include "Render/RenderPass/Public/LightPass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/AmbientLightComponent.h"
#include "component/Public/DirectionalLightComponent.h"
#include "component/Public/PointLightComponent.h"
#include "Component/Public/SpotLightComponent.h"

FLightPass::FLightPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera) :
	FRenderPass(InPipeline, InConstantBufferCamera, nullptr)
{
	GlobalLightConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FGlobalLightConstant>();
	PointLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(PointLightBufferCount);
	SpotLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FSpotLightInfo>(SpotLightBufferCount);
	ClusterAABB = FRenderResourceFactory::CreateRWStructuredBuffer<FAABB>(GetClusterCount());
}
void FLightPass::Execute(FRenderingContext& Context)
{
	// Setup lighting constant buffer from scene lights
	FGlobalLightConstant GlobalLightData = {};
	TArray<FPointLightInfo> PointLightDatas;
	TArray<FSpotLightInfo> SpotLightDatas;
	// 수정 필요: Context에서 가져오기
	if (!Context.AmbientLights.empty()) {
		UAmbientLightComponent* AmbLight = Context.AmbientLights[0];
		GlobalLightData.Ambient = AmbLight->GetAmbientLightInfo();
	}

	if (!Context.DirectionalLights.empty()) {
		UDirectionalLightComponent* DirLight = Context.DirectionalLights[0];
		GlobalLightData.Directional = DirLight->GetDirectionalLightInfo();
	}

	// Fill point lights from scene
	int PointLightComponentCount = Context.PointLights.size();
	PointLightDatas.reserve(PointLightComponentCount);
	for (int32 i = 0; i < PointLightComponentCount; ++i)
	{
		UPointLightComponent* Light = Context.PointLights[i];
		if (!Light || !Light->GetVisible()) continue;
		PointLightDatas.push_back(Light->GetPointlightInfo());
	}
	// 5. Spot Lights 배열 채우기 (최대 NUM_SPOT_LIGHT개)
	int SpotLightComponentCount = Context.SpotLights.size();
	SpotLightDatas.reserve(SpotLightComponentCount);
	int CurSpotLightIdx = 0;
	for (int32 i = 0; i < SpotLightComponentCount; ++i)
	{
		USpotLightComponent* Light = Context.SpotLights[i];
		if (!Light->GetVisible()) continue;
		SpotLightDatas.push_back(Light->GetSpotLightInfo());
	}

	int PointLightCount = PointLightDatas.size();
	int SpotLightCount = SpotLightDatas.size();
	//최대갯수 재할당
	if (PointLightBufferCount < PointLightCount)
	{
		while (PointLightBufferCount < PointLightCount)
		{
			PointLightBufferCount = PointLightBufferCount << 1;
		}
		PointLightStructuredBuffer->Release();
		PointLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(PointLightBufferCount);
	}
	if (SpotLightBufferCount < SpotLightCount)
	{
		while (SpotLightBufferCount < SpotLightCount)
		{
			SpotLightBufferCount = SpotLightBufferCount << 1;
		}
		SpotLightStructuredBuffer->Release();
		SpotLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(SpotLightBufferCount);
	}

	FRenderResourceFactory::UpdateConstantBufferData(GlobalLightConstantBuffer, GlobalLightData);
	FRenderResourceFactory::UpdateStructuredBuffer(PointLightStructuredBuffer, PointLightDatas);
	FRenderResourceFactory::UpdateStructuredBuffer(SpotLightStructuredBuffer, SpotLightDatas);

	/*Pipeline->SetConstantBuffer(3, true, ConstantBufferLighting);
	Pipeline->SetConstantBuffer(3, false, ConstantBufferLighting);*/
}


void FLightPass::Release()
{

}