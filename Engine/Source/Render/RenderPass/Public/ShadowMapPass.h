#pragma once
#include "Render/RenderPass/Public/RenderPass.h"
#include "Texture/Public/ShadowMapResources.h"
#include "Global/Types.h"
#include "Render/RenderPass/Public/ShadowData.h"
#include "Manager/Render/Public/CascadeManager.h"

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
		ID3D11PixelShader* InDepthOnlyPS,
		ID3D11InputLayout* InDepthOnlyInputLayout,
		ID3D11VertexShader* InPointLightShadowVS,
		ID3D11PixelShader* InPointLightShadowPS,
		ID3D11InputLayout* InPointLightShadowInputLayout);

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

	/**
	 * @brief Spot light의 shadow map 리소스를 가져옵니다.
	 * @param Light Spot light component
	 * @return Shadow map 리소스 포인터 (없으면 nullptr)
	 */
	FShadowMapResource* GetSpotShadowMap(USpotLightComponent* Light);

	/**
	 * @brief Point light의 cube shadow map 리소스를 가져옵니다.
	 * @param Light Point light component
	 * @return Cube shadow map 리소스 포인터 (없으면 nullptr)
	 */
	FCubeShadowMapResource* GetPointShadowMap(UPointLightComponent* Light);

	FShadowMapResource* GetShadowAtlas();

	/**
	 * @brief Directional Light의 Atlas Tile 위치를 가져옵니다.
	 * @param Index Directional Light의 인덱스
	 * @return Directional Light의 Atlas Tile 위치
	 * @todo ShadowMapPass에 저장되어있는 인덱스와 외부에서 활용되는 인덱스 일치여부 확인
	 */
	FShadowAtlasTilePos GetDirectionalAtlasTilePos(uint32 Index) const;

	/**
	 * @brief Spotlight의 Atlas Tile 위치를 가져옵니다.
	 * @param Index Spotlight의 인덱스
	 * @return Spotlight의 Atlas Tile 위치
	 * @todo ShadowMapPass에 저장되어있는 인덱스와 외부에서 활용되는 인덱스 일치여부 확인
	 */
	FShadowAtlasTilePos GetSpotAtlasTilePos(uint32 Index) const;

	// Shadow stat information
	uint64 GetTotalShadowMapMemory() const;
	uint32 GetUsedAtlasTileCount() const;
	static uint32 GetMaxAtlasTileCount();

	/**
	 * @brief Point Light의 Atlas Tile 위치를 가져옵니다.
	 * @param Index Point Light의 인덱스
	 * @return Point Light의 Atlas Tile 위치
	 * @todo ShadowMapPass에 저장되어있는 인덱스와 외부에서 활용되는 인덱스 일치여부 확인
	 */
	FShadowAtlasPointLightTilePos GetPointAtlasTilePos(uint32 Index) const;

private:
	// --- Directional Light Shadow Rendering ---
	/**
	 * @brief Directional light의 shadow map을 렌더링합니다.
	 * @param Light Directional light component
	 * @param Meshes 렌더링할 static mesh 목록
	 */
	void RenderDirectionalShadowMap(
		UDirectionalLightComponent* Light,
		const TArray<UStaticMeshComponent*>& Meshes,
		UCamera* InCamera
		);

	// --- Spot Light Shadow Rendering ---
	/**
	 * @brief Spot light의 shadow map을 렌더링합니다.
	 * @param Light Spot light component
	 * @param Meshes 렌더링할 static mesh 목록
	 */
	void RenderSpotShadowMap(
		USpotLightComponent* Light,
		uint32 AtlasIndex,
		const TArray<UStaticMeshComponent*>& Meshes
		);

	// --- Point Light Shadow Rendering (6 faces) ---
	/**
	 * @brief Point light의 cube shadow map을 렌더링합니다 (6면).
	 * @param Light Point light component
	 * @param Meshes 렌더링할 static mesh 목록
	 */
	void RenderPointShadowMap(
		UPointLightComponent* Light,
		uint32 AtlasIndex,
		const TArray<UStaticMeshComponent*>& Meshes
		);

	void SetShadowAtlasTilePositionStructuredBuffer();

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
	 * @param Meshes 렌더링할 메시 목록 (현재 사용 안 함, Directional과 시그니처 일관성 유지)
	 * @param OutView 출력 view matrix
	 * @param OutProj 출력 projection matrix
	 */
	void CalculateSpotLightViewProj(USpotLightComponent* Light,
		const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj);

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

	void RenderMeshDepth(const UStaticMeshComponent* InMesh, const FMatrix& InView, const FMatrix& InProj) const;

	// /**
	//  * @brief Directional light의 rasterizer state를 가져오거나 생성합니다.
	//  *
	//  * Light별로 DepthBias/SlopeScaledDepthBias가 다르므로, 각 light마다
	//  * 전용 rasterizer state를 캐싱합니다. 매 프레임 생성/해제를 방지하여 성능 향상.
	//  *
	//  * @param Light Directional light component
	//  * @return Light 전용 rasterizer state
	//  */
	// ID3D11RasterizerState* GetOrCreateRasterizerState(UDirectionalLightComponent* Light);
	//
	// /**
	//  * @brief Spot light의 rasterizer state를 가져오거나 생성합니다.
	//  *
	//  * Light별로 DepthBias/SlopeScaledDepthBias가 다르므로, 각 light마다
	//  * 전용 rasterizer state를 캐싱합니다. 매 프레임 생성/해제를 방지하여 성능 향상.
	//  *
	//  * @param Light Spot light component
	//  * @return Light 전용 rasterizer state
	//  */
	// ID3D11RasterizerState* GetOrCreateRasterizerState(USpotLightComponent* Light);
	//
	// /**
	//  * @brief Point light의 rasterizer state를 가져오거나 생성합니다.
	//  *
	//  * Light별로 DepthBias/SlopeScaledDepthBias가 다르므로, 각 light마다
	//  * 전용 rasterizer state를 캐싱합니다. 매 프레임 생성/해제를 방지하여 성능 향상.
	//  *
	//  * @param Light Point light component
	//  * @return Light 전용 rasterizer state
	//  */
	// ID3D11RasterizerState* GetOrCreateRasterizerState(UPointLightComponent* Light);

	ID3D11RasterizerState* GetOrCreateRasterizerState(
		float InShadowBias,
		float InShadowSlopBias
		);

private:
	// Shaders
	ID3D11VertexShader* DepthOnlyVS = nullptr;
	ID3D11PixelShader* DepthOnlyPS = nullptr;
	ID3D11InputLayout* DepthOnlyInputLayout = nullptr;

	// Point Light Shadow Shaders (with linear distance)
	ID3D11VertexShader* LinearDepthOnlyVS = nullptr;
	ID3D11PixelShader* LinearDepthOnlyPS = nullptr;
	ID3D11InputLayout* PointLightShadowInputLayout = nullptr;

	// States
	ID3D11DepthStencilState* ShadowDepthStencilState = nullptr;
	ID3D11RasterizerState* ShadowRasterizerState = nullptr;

	// Shadow map 리소스 관리 (동적 할당)
	TMap<UDirectionalLightComponent*, FShadowMapResource*> DirectionalShadowMaps;
	TMap<USpotLightComponent*, FShadowMapResource*> SpotShadowMaps;
	TMap<UPointLightComponent*, FCubeShadowMapResource*> PointShadowMaps;

	// Rasterizer state 캐싱 (매 프레임 생성/해제 방지)
	// TMap<UDirectionalLightComponent*, ID3D11RasterizerState*> DirectionalRasterizerStates;
	// TMap<USpotLightComponent*, ID3D11RasterizerState*> SpotRasterizerStates;
	// TMap<UPointLightComponent*, ID3D11RasterizerState*> PointRasterizerStates;

	TMap<FString, ID3D11RasterizerState*> LightRasterizerStates;

	// Rasterizer State 캐싱 (Bias, Splop Bias를 키값으로 접근)

	// Constant buffers (DepthOnlyVS.hlsl의 ViewProj와 동일)
	ID3D11Buffer* ShadowViewProjConstantBuffer = nullptr;
	ID3D11Buffer* PointLightShadowParamsBuffer = nullptr;

	TArray<FShadowAtlasTilePos> ShadowAtlasDirectionalLightTilePosArray;
	TArray<FShadowAtlasTilePos> ShadowAtlasSpotLightTilePosArray;
	TArray<FShadowAtlasPointLightTilePos> ShadowAtlasPointLightTilePosArray;

	// 실제 렌더링된 라이트 개수 및 타일 개수
	uint32 ActiveDirectionalLightCount = 0; // Directional Light 개수 (0 또는 1)
	uint32 ActiveDirectionalCascadeCount = 0; // CSM Cascade Count (1 ~ 8)
	uint32 ActiveSpotLightCount = 0;
	uint32 ActivePointLightCount = 0;

	ID3D11Buffer* ShadowAtlasDirectionalLightTilePosStructuredBuffer = nullptr;
	ID3D11ShaderResourceView* ShadowAtlasDirectionalLightTilePosStructuredSRV = nullptr;
	
	ID3D11Buffer* ShadowAtlasSpotLightTilePosStructuredBuffer = nullptr;
	ID3D11ShaderResourceView* ShadowAtlasSpotLightTilePosStructuredSRV = nullptr;
	
	ID3D11Buffer* ShadowAtlasPointLightTilePosStructuredBuffer = nullptr;
	ID3D11ShaderResourceView* ShadowAtlasPointLightTilePosStructuredSRV = nullptr;

	FShadowMapResource ShadowAtlas{};

	// Handle Cascade Data
	ID3D11Buffer* ConstantCascadeData = nullptr;
};
