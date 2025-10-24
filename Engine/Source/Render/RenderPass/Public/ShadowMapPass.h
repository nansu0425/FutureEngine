#pragma once
#include "Render/RenderPass/Public/RenderPass.h"
#include "Texture/Public/ShadowMapResources.h"
#include "Global/Types.h"

class ULightComponent;
class UDirectionalLightComponent;
class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * @brief Shadow map 렌더링 전용 pass
 *
 * 각 라이트의 shadow map을 렌더링합니다:
 * - Directional Light: 단일 shadow map (orthographic projection)
 * - Spot Light: 단일 shadow map (perspective projection)
 * - Point Light: Cube shadow map (6면, omnidirectional)
 *
 * StaticMeshPass 이전에 실행되어 depth map을 준비합니다.
 */
class FShadowMapPass : public FRenderPass
{
public:
	FShadowMapPass(UPipeline* InPipeline,
		ID3D11Buffer* InConstantBufferCamera,
		ID3D11Buffer* InConstantBufferModel,
		ID3D11VertexShader* InDepthOnlyVS,
		ID3D11InputLayout* InDepthOnlyInputLayout);

	virtual ~FShadowMapPass();

	void Execute(FRenderingContext& Context) override;
	void Release() override;

	// --- Public Getters ---
	/**
	 * @brief Directional light의 shadow map 리소스를 가져옵니다.
	 * @param Light Directional light component
	 * @return Shadow map 리소스 포인터 (없으면 nullptr)
	 */
	FShadowMapResource* GetDirectionalShadowMap(UDirectionalLightComponent* Light);

private:
	// --- Directional Light Shadow Rendering ---
	/**
	 * @brief Directional light의 shadow map을 렌더링합니다.
	 * @param Light Directional light component
	 * @param Meshes 렌더링할 static mesh 목록
	 */
	void RenderDirectionalShadowMap(UDirectionalLightComponent* Light,
		const TArray<UStaticMeshComponent*>& Meshes);

	// --- Spot Light Shadow Rendering ---
	/**
	 * @brief Spot light의 shadow map을 렌더링합니다.
	 * @param Light Spot light component
	 * @param Meshes 렌더링할 static mesh 목록
	 */
	void RenderSpotShadowMap(USpotLightComponent* Light,
		const TArray<UStaticMeshComponent*>& Meshes);

	// --- Point Light Shadow Rendering (6 faces) ---
	/**
	 * @brief Point light의 cube shadow map을 렌더링합니다 (6면).
	 * @param Light Point light component
	 * @param Meshes 렌더링할 static mesh 목록
	 */
	void RenderPointShadowMap(UPointLightComponent* Light,
		const TArray<UStaticMeshComponent*>& Meshes);

	// --- Helper Functions ---
	/**
	 * @brief Directional light의 view-projection 행렬을 계산합니다.
	 * @param Light Directional light component
	 * @param Meshes 렌더링할 메시 목록 (AABB 계산용)
	 * @param OutView 출력 view matrix
	 * @param OutProj 출력 projection matrix
	 */
	void CalculateDirectionalLightViewProj(UDirectionalLightComponent* Light,
		const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj);

	/**
	 * @brief Spot light의 view-projection 행렬을 계산합니다.
	 * @param Light Spot light component
	 * @return Light space view-projection 행렬
	 */
	FMatrix CalculateSpotLightViewProj(USpotLightComponent* Light);

	/**
	 * @brief Point light의 6면에 대한 view-projection 행렬을 계산합니다.
	 * @param Light Point light component
	 * @param OutViewProj 출력 배열 (6개)
	 */
	void CalculatePointLightViewProj(UPointLightComponent* Light, FMatrix OutViewProj[6]);

	/**
	 * @brief Shadow map 리소스를 가져오거나 생성합니다.
	 * @param Light Light component
	 * @return Shadow map 리소스 포인터
	 */
	FShadowMapResource* GetOrCreateShadowMap(ULightComponent* Light);

	/**
	 * @brief Cube shadow map 리소스를 가져오거나 생성합니다.
	 * @param Light Point light component
	 * @return Cube shadow map 리소스 포인터
	 */
	FCubeShadowMapResource* GetOrCreateCubeShadowMap(UPointLightComponent* Light);

	/**
	 * @brief 메시를 shadow depth로 렌더링합니다.
	 * @param Mesh Static mesh component
	 * @param View Light space view 행렬
	 * @param Proj Light space projection 행렬
	 */
	void RenderMeshDepth(UStaticMeshComponent* Mesh, const FMatrix& View, const FMatrix& Proj);

private:
	// Shaders
	ID3D11VertexShader* DepthOnlyShader = nullptr;
	ID3D11InputLayout* DepthOnlyInputLayout = nullptr;

	// States
	ID3D11DepthStencilState* ShadowDepthStencilState = nullptr;
	ID3D11RasterizerState* ShadowRasterizerState = nullptr;

	// Shadow map 리소스 관리 (동적 할당)
	TMap<UDirectionalLightComponent*, FShadowMapResource*> DirectionalShadowMaps;
	TMap<USpotLightComponent*, FShadowMapResource*> SpotShadowMaps;
	TMap<UPointLightComponent*, FCubeShadowMapResource*> PointShadowMaps;

	// Constant buffers (DepthOnlyVS.hlsl의 ViewProj와 동일)
	struct FShadowViewProjConstant
	{
		FMatrix ViewProjection;
	};
	ID3D11Buffer* ShadowViewProjConstantBuffer = nullptr;
};
