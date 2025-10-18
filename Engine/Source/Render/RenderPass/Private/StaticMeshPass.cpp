#include "pch.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/AmbientLightComponent.h"
#include "component/Public/DirectionalLightComponent.h"
#include "component/Public/PointLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"

FStaticMeshPass::FStaticMeshPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferCamera, ID3D11Buffer* InConstantBufferModel,
	ID3D11Buffer* InConstantBufferLighting,
	ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS)
	: FRenderPass(InPipeline, InConstantBufferCamera, InConstantBufferModel), VS(InVS), PS(InPS), InputLayout(InLayout), DS(InDS),
	ConstantBufferLighting(InConstantBufferLighting)
{
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None; RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);
	FPipelineInfo PipelineInfo = { InputLayout, VS, RS, DS, PS, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	// Set a default sampler to slot 0 to ensure one is always bound
	Pipeline->SetSamplerState(0, false, URenderer::GetInstance().GetDefaultSampler());

	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferCamera);
	Pipeline->SetConstantBuffer(1, false, ConstantBufferCamera);

	// Setup lighting constant buffer from scene lights
	FLightingConstants LightingData = {};
	
	// 수정 필요: Context에서 가져오기
	if (!Context.AmbientLights.empty()) {
		UAmbientLightComponent* AmbLight = Context.AmbientLights[0];
		FVector color = AmbLight->GetLightColor();
		LightingData.Ambient.Color = FVector4(color.X, color.Y, color.Z, 1.0f);
		LightingData.Ambient.Intensity = AmbLight->GetIntensity();

		//UE_LOG("Ambient: intensity = %f", AmbLight->GetIntensity());
	}

	if (!Context.DirectionalLights.empty()) {
		UDirectionalLightComponent* DirLight = Context.DirectionalLights[0];
		FVector color = DirLight->GetLightColor();
		FVector dir = DirLight->GetForwardVector();  // 방향 가져오기
		LightingData.Directional.Color = FVector4(color.X, color.Y, color.Z, 1.0f);
		LightingData.Directional.Direction = FVector(dir.X, dir.Y, dir.Z);
		LightingData.Directional.Intensity = DirLight->GetIntensity();
	}
	
	// Fill point lights from scene
	int32 PointLightCount = std::min((int32)Context.PointLights.size(), NUM_POINT_LIGHT);
	for (int32 i = 0; i < PointLightCount; ++i)
	{
		UPointLightComponent* Light = Context.PointLights[i];
		if (!Light || !Light->GetVisible()) continue;
		
		FVector LightColor = Light->GetLightColor();
		FVector LightPos = Light->GetWorldLocation();
		
		LightingData.PointLights[i].Color = FVector4(LightColor.X, LightColor.Y, LightColor.Z, 1.0f);
		LightingData.PointLights[i].Position = FVector(LightPos.X, LightPos.Y, LightPos.Z);
		LightingData.PointLights[i].Range = Light->GetAttenuationRadius();
		LightingData.PointLights[i].Intensity = Light->GetIntensity();
		LightingData.PointLights[i].DistanceFalloffExponent = Light->GetDistanceFalloffExponent();
	}
	// 5. Spot Lights 배열 채우기 (최대 NUM_SPOT_LIGHT개)
	int32 SpotLightCount = std::min((int32)Context.SpotLights.size(), NUM_SPOT_LIGHT);
	for (int32 i = 0; i < SpotLightCount; ++i)
	{
		USpotLightComponent* Light = Context.SpotLights[i];
		if (!Light->GetVisible()) continue;
    
		FVector color = Light->GetLightColor();
		FVector pos = Light->GetWorldLocation();
		FVector dir = Light->GetForwardVector();
    
		LightingData.SpotLights[i].Color = FVector4(color.X, color.Y, color.Z, 1.0f);
		LightingData.SpotLights[i].Position = FVector(pos.X, pos.Y, pos.Z);
		LightingData.SpotLights[i].Direction = FVector(dir.X, dir.Y, dir.Z);
		LightingData.SpotLights[i].Range = Light->GetAttenuationRadius();
		LightingData.SpotLights[i].InnerConeAngle = Light->GetAttenuationAngle();
		LightingData.SpotLights[i].OuterConeAngle = Light->GetAngleFalloffExponent();
		LightingData.SpotLights[i].Intensity = Light->GetIntensity();
	}

	LightingData.NumPointLights = PointLightCount;
	LightingData.NumSpotLights = SpotLightCount;
	
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLighting, LightingData);
	Pipeline->SetConstantBuffer(3, false, ConstantBufferLighting);
	
	if (!(Context.ShowFlags & EEngineShowFlags::SF_StaticMesh)) { return; }
	TArray<UStaticMeshComponent*>& MeshComponents = Context.StaticMeshes;
	sort(MeshComponents.begin(), MeshComponents.end(),
		[](UStaticMeshComponent* A, UStaticMeshComponent* B) {
			int32 MeshA = A->GetStaticMesh() ? A->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			int32 MeshB = B->GetStaticMesh() ? B->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			return MeshA < MeshB;
		});

	FStaticMesh* CurrentMeshAsset = nullptr;
	UMaterial* CurrentMaterial = nullptr;

	// --- RTVs Setup ---
	
	/**
	 * @todo Find a better way to reduce depdency upon Renderer class.
	 * @note How about introducing methods like BeginPass(), EndPass() to set up and release pass specific state?
	 */
	const auto& Renderer = URenderer::GetInstance();
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
	ID3D11RenderTargetView* RTVs[2] = { RTV, DeviceResources->GetNormalRenderTargetView() };
	ID3D11DepthStencilView* DSV = DeviceResources->GetDepthStencilView();
	Pipeline->SetRenderTargets(2, RTVs, DSV);

	// --- RTVs Setup End ---

	for (UStaticMeshComponent* MeshComp : MeshComponents) 
	{
		if (!MeshComp->GetStaticMesh()) { continue; }
		FStaticMesh* MeshAsset = MeshComp->GetStaticMesh()->GetStaticMeshAsset();
		if (!MeshAsset) { continue; }

		if (CurrentMeshAsset != MeshAsset)
		{
			Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
			CurrentMeshAsset = MeshAsset;
		}
		
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, MeshComp->GetWorldTransformMatrix());
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		if (MeshAsset->MaterialInfo.empty() || MeshComp->GetStaticMesh()->GetNumMaterials() == 0) 
		{
			Pipeline->DrawIndexed(MeshAsset->Indices.size(), 0, 0);
			continue;
		}

		if (MeshComp->IsScrollEnabled()) 
		{
			MeshComp->SetElapsedTime(MeshComp->GetElapsedTime() + UTimeManager::GetInstance().GetDeltaTime());
		}

		for (const FMeshSection& Section : MeshAsset->Sections)
		{
			UMaterial* Material = MeshComp->GetMaterial(Section.MaterialSlot);
			if (CurrentMaterial != Material) {
				FMaterialConstants MaterialConstants = {};
				FVector AmbientColor = Material->GetAmbientColor(); MaterialConstants.Ka = FVector4(AmbientColor.X, AmbientColor.Y, AmbientColor.Z, 1.0f);
				FVector DiffuseColor = Material->GetDiffuseColor(); MaterialConstants.Kd = FVector4(DiffuseColor.X, DiffuseColor.Y, DiffuseColor.Z, 1.0f);
				FVector SpecularColor = Material->GetSpecularColor(); MaterialConstants.Ks = FVector4(SpecularColor.X, SpecularColor.Y, SpecularColor.Z, 1.0f);
				MaterialConstants.Ns = Material->GetSpecularExponent();
				MaterialConstants.Ni = Material->GetRefractionIndex();
				MaterialConstants.D = Material->GetDissolveFactor();
				MaterialConstants.MaterialFlags = 0;
				if (Material->GetDiffuseTexture())  { MaterialConstants.MaterialFlags |= HAS_DIFFUSE_MAP; }
				if (Material->GetAmbientTexture())  { MaterialConstants.MaterialFlags |= HAS_AMBIENT_MAP; }
				if (Material->GetSpecularTexture()) { MaterialConstants.MaterialFlags |= HAS_SPECULAR_MAP; }
				if (Material->GetNormalTexture())   { MaterialConstants.MaterialFlags |= HAS_NORMAL_MAP; }
				if (Material->GetAlphaTexture())    { MaterialConstants.MaterialFlags |= HAS_ALPHA_MAP; }
				if (Material->GetBumpTexture())     { MaterialConstants.MaterialFlags |= HAS_BUMP_MAP; }
				MaterialConstants.Time = MeshComp->GetElapsedTime();

				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);

				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					Pipeline->SetTexture(0, false, DiffuseTexture->GetTextureSRV());
					Pipeline->SetSamplerState(0, false, DiffuseTexture->GetTextureSampler());
				}
				if (UTexture* AmbientTexture = Material->GetAmbientTexture())
				{
					Pipeline->SetTexture(1, false, AmbientTexture->GetTextureSRV());
				}
				if (UTexture* SpecularTexture = Material->GetSpecularTexture())
				{
					Pipeline->SetTexture(2, false, SpecularTexture->GetTextureSRV());
				}
				if (UTexture* NormalTexture = Material->GetNormalTexture())
				{
					Pipeline->SetTexture(3, false, NormalTexture->GetTextureSRV());
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					Pipeline->SetTexture(4, false, AlphaTexture->GetTextureSRV());
				}
				
				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
	Pipeline->SetConstantBuffer(2, false, nullptr);

	
	// --- RTVs Reset ---
	
	/**
	 * @todo Find a better way to reduce depdency upon Renderer class.
	 * @note How about introducing methods like BeginPass(), EndPass() to set up and release pass specific state?
	 */
	Pipeline->SetRenderTargets(1, RTVs, DSV);

	// --- RTVs Reset End ---
}

void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
}
