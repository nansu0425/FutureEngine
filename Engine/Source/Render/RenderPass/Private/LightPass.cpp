#include "pch.h"
#include "Render/RenderPass/Public/LightPass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/AmbientLightComponent.h"
#include "component/Public/DirectionalLightComponent.h"
#include "component/Public/PointLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Source/Editor/Public/Camera.h"

FLightPass::FLightPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera) :
	FRenderPass(InPipeline, InConstantBufferCamera, nullptr)
{
	ViewClusterInfoConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<ViewClusterInfo>();
	GlobalLightConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FGlobalLightConstant>();
	PointLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(PointLightBufferCount);
	SpotLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FSpotLightInfo>(SpotLightBufferCount);
	ClusterAABBRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(24, GetClusterCount());
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ViewClusterCS.hlsl", &ViewClusterCS);

	FRenderResourceFactory::CreateStructuredShaderResourceView(PointLightStructuredBuffer, &PointLightStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(SpotLightStructuredBuffer, &SpotLightStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(ClusterAABBRWStructuredBuffer, &ClusterAABBRWStructuredBufferSRV);
	FRenderResourceFactory::CreateUnorderedAccessView(ClusterAABBRWStructuredBuffer, &ClusterAABBRWStructuredBufferUAV);
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

	Pipeline->SetConstantBuffer(0, EShaderType::CS, ViewClusterInfoConstantBuffer);

	FMatrix ProjectionInv = Context.CurrentCamera->GetFViewProjConstantsInverse().Projection;
	float CamNear = Context.CurrentCamera->GetNearZ();
	float CamFar = Context.CurrentCamera->GetFarZ();
	FRenderResourceFactory::UpdateConstantBufferData(ViewClusterInfoConstantBuffer,
		ViewClusterInfo{ ProjectionInv, CamNear,CamFar, ScreenXSlideNum, ScreenYSlideNum, ZSlideNum });
	Pipeline->SetUnorderedAccessView(0, ClusterAABBRWStructuredBufferUAV);

	Pipeline->DispatchCS(ViewClusterCS, ScreenXSlideNum, ScreenYSlideNum, ZSlideNum);

}


void FLightPass::Release()
{

}