#include "pch.h"
#include "Render/RenderPass/Public/LightPass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/AmbientLightComponent.h"
#include "component/Public/DirectionalLightComponent.h"
#include "component/Public/PointLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Source/Editor/Public/Camera.h"
FLightPass::FLightPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, 
	ID3D11InputLayout* InGizmoInputLayout, ID3D11VertexShader* InGizmoVS, ID3D11PixelShader* InGizmoPS,
	ID3D11DepthStencilState* InGizmoDSS) :
	FRenderPass(InPipeline, InConstantBufferCamera, nullptr)
{
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ViewClusterCS.hlsl", &ViewClusterCS);
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ClusteredLightCullingCS.hlsl", &ClusteredLightCullingCS);
	FRenderResourceFactory::CreateComputeShader(L"Asset/Shader/ClusterGizmoSetCS.hlsl", &ClusterGizmoSetCS);

	ViewClusterInfoConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FViewClusterInfo>();
	LightCountInfoConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FLightCountInfo>();
	GlobalLightConstantBuffer = FRenderResourceFactory::CreateConstantBuffer<FGlobalLightConstant>();
	PointLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FPointLightInfo>(PointLightBufferCount);
	SpotLightStructuredBuffer = FRenderResourceFactory::CreateStructuredBuffer<FSpotLightInfo>(SpotLightBufferCount);
	ClusterAABBRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(24, GetClusterCount());
	LightIndicesRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(4, GetClusterCount() * LightMaxCountPerCluster);

	FRenderResourceFactory::CreateStructuredShaderResourceView(PointLightStructuredBuffer, &PointLightStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(SpotLightStructuredBuffer, &SpotLightStructuredBufferSRV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(ClusterAABBRWStructuredBuffer, &ClusterAABBRWStructuredBufferSRV);
	FRenderResourceFactory::CreateUnorderedAccessView(ClusterAABBRWStructuredBuffer, &ClusterAABBRWStructuredBufferUAV);	
	FRenderResourceFactory::CreateStructuredShaderResourceView(LightIndicesRWStructuredBuffer, &LightIndicesRWStructuredBufferSRV);
	FRenderResourceFactory::CreateUnorderedAccessView(LightIndicesRWStructuredBuffer, &LightIndicesRWStructuredBufferUAV);

	//Cluster Gizmo (Pos, Color) = 28
	//Cube = 8 Vertex, 24 Index
	ClusterGizmoVertexRWStructuredBuffer = FRenderResourceFactory::CreateRWStructuredBuffer(28, (GetClusterCount() + 1) * 8);
	FRenderResourceFactory::CreateUnorderedAccessView(ClusterGizmoVertexRWStructuredBuffer,  &ClusterGizmoVertexRWStructuredBufferUAV);
	FRenderResourceFactory::CreateStructuredShaderResourceView(ClusterGizmoVertexRWStructuredBuffer,  &ClusterGizmoVertexRWStructuredBufferSRV);

	GizmoInputLayout = InGizmoInputLayout;
	GizmoVS = InGizmoVS;
	GizmoPS = InGizmoPS;
	GizmoDSS = InGizmoDSS;
	CameraConstantBuffer = InConstantBufferCamera;

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

	uint32 PointLightCount = PointLightDatas.size();
	uint32 SpotLightCount = SpotLightDatas.size();
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

	//Cluster AABB Set
	FCameraConstants Inv = Context.CurrentCamera->GetFViewProjConstantsInverse();
	FMatrix ProjectionInv = Inv.Projection;
	FMatrix ViewInv = Inv.View;
	FMatrix ViewMatrix = Context.CurrentCamera->GetFViewProjConstants().View;
	float CamNear = Context.CurrentCamera->GetNearZ();
	float CamFar = Context.CurrentCamera->GetFarZ();
	float Aspect = Context.CurrentCamera->GetAspect();
	float fov = Context.CurrentCamera->GetFovY();
	FRenderResourceFactory::UpdateConstantBufferData(ViewClusterInfoConstantBuffer,
		FViewClusterInfo{ ProjectionInv, ViewInv, ViewMatrix, CamNear,CamFar,Aspect,fov, ScreenXSlideNum, ScreenYSlideNum, ZSlideNum, LightMaxCountPerCluster });
	FRenderResourceFactory::UpdateConstantBufferData(LightCountInfoConstantBuffer,
		FLightCountInfo{ PointLightCount, SpotLightCount });
	Pipeline->SetConstantBuffer(0, EShaderType::CS, ViewClusterInfoConstantBuffer);
	Pipeline->SetConstantBuffer(1, EShaderType::CS, LightCountInfoConstantBuffer);
	Pipeline->SetUnorderedAccessView(0, ClusterAABBRWStructuredBufferUAV);
	Pipeline->DispatchCS(ViewClusterCS, ScreenXSlideNum, ScreenYSlideNum, ZSlideNum);

	//Light 분류
	Pipeline->SetUnorderedAccessView(0, LightIndicesRWStructuredBufferUAV);
	Pipeline->SetShaderResourceView(0, EShaderType::CS, ClusterAABBRWStructuredBufferSRV);
	Pipeline->SetShaderResourceView(1, EShaderType::CS, PointLightStructuredBufferSRV);
	Pipeline->SetShaderResourceView(2, EShaderType::CS, SpotLightStructuredBufferSRV);
	Pipeline->DispatchCS(ClusteredLightCullingCS, ScreenXSlideNum, ScreenYSlideNum, ZSlideNum);



	//클러스터 기즈모 제작
	if (bClusterGizmoSet == false) 
	{
		bClusterGizmoSet = true;
		Pipeline->SetUnorderedAccessView(0, ClusterGizmoVertexRWStructuredBufferUAV);
		Pipeline->SetShaderResourceView(0, EShaderType::CS, ClusterAABBRWStructuredBufferSRV);
		Pipeline->SetShaderResourceView(1, EShaderType::CS, PointLightStructuredBufferSRV);
		Pipeline->SetShaderResourceView(2, EShaderType::CS, SpotLightStructuredBufferSRV);
		Pipeline->SetShaderResourceView(3, EShaderType::CS, LightIndicesRWStructuredBufferSRV);
		Pipeline->DispatchCS(ClusterGizmoSetCS, ScreenXSlideNum, ScreenYSlideNum, ZSlideNum);
	}
	
	
	//클러스터 기즈모 출력
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(FRenderState());
	FPipelineInfo PipelineInfo = { nullptr, GizmoVS, RS, GizmoDSS, GizmoPS, nullptr, D3D11_PRIMITIVE_TOPOLOGY_LINELIST };
	Pipeline->UpdatePipeline(PipelineInfo);
	Pipeline->SetConstantBuffer(1, EShaderType::VS, ConstantBufferCamera);

	Pipeline->SetShaderResourceView(0, EShaderType::VS, ClusterGizmoVertexRWStructuredBufferSRV);

	URenderer& Renderer = URenderer::GetInstance();
	const auto& DeviceResources = Renderer.GetDeviceResources();
	ID3D11RenderTargetView* RTV = nullptr;
	if (Renderer.GetFXAA())
	{
		RTV = DeviceResources->GetSceneColorRenderTargetView();
	}
	else
	{
		RTV = DeviceResources->GetRenderTargetView();
	}
	ID3D11RenderTargetView* RTVs[] = { RTV};
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
	Pipeline->SetRenderTargets(1, RTVs, DSV);
	Pipeline->Draw((GetClusterCount() + 1) * 24, 0);




}


void FLightPass::Release()
{

}