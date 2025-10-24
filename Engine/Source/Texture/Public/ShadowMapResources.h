#pragma once
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

/**
 * @brief 단일 shadow map 리소스 (Directional/Spot Light용)
 *
 * Shadow map을 렌더링하기 위한 depth texture와 뷰를 관리합니다.
 * - Depth Stencil View (DSV): Shadow 렌더링 시 depth 쓰기
 * - Shader Resource View (SRV): 메인 렌더링에서 shadow sampling
 */
struct FShadowMapResource
{
	ComPtr<ID3D11Texture2D> ShadowTexture = nullptr;
	ComPtr<ID3D11DepthStencilView> ShadowDSV = nullptr;
	ComPtr<ID3D11ShaderResourceView> ShadowSRV = nullptr;
	D3D11_VIEWPORT ShadowViewport = {};
	uint32 Resolution = 1024;

	/**
	 * @brief Shadow map 리소스를 초기화합니다.
	 * @param Device DirectX 11 device
	 * @param InResolution Shadow map 해상도 (정사각형)
	 */
	void Initialize(ID3D11Device* Device, uint32 InResolution);

	/**
	 * @brief 모든 리소스를 해제합니다.
	 */
	void Release();

	/**
	 * @brief 해상도 변경이 필요한지 확인합니다.
	 * @param NewResolution 새로운 해상도
	 * @return 해상도가 다르면 true
	 */
	bool NeedsResize(uint32 NewResolution) const { return Resolution != NewResolution; }

	/**
	 * @brief 리소스가 유효한지 확인합니다.
	 * @return 모든 리소스가 생성되었으면 true
	 */
	bool IsValid() const { return ShadowTexture != nullptr && ShadowDSV != nullptr && ShadowSRV != nullptr; }
};

/**
 * @brief Cube shadow map 리소스 (Point Light용 - 6면)
 *
 * Point light의 omnidirectional shadow를 위한 cube texture와 뷰를 관리합니다.
 * - 6개의 Depth Stencil View (DSV): 각 면을 개별적으로 렌더링
 * - 1개의 Shader Resource View (SRV): Cube texture로 shadow sampling
 */
struct FCubeShadowMapResource
{
	ComPtr<ID3D11Texture2D> ShadowTextureCube = nullptr;
	ComPtr<ID3D11DepthStencilView> ShadowDSVs[6] = { nullptr };  // +X, -X, +Y, -Y, +Z, -Z
	ComPtr<ID3D11ShaderResourceView> ShadowSRV = nullptr;        // Cube texture SRV
	D3D11_VIEWPORT ShadowViewport = {};
	uint32 Resolution = 512;

	/**
	 * @brief Cube shadow map 리소스를 초기화합니다.
	 * @param Device DirectX 11 device
	 * @param InResolution 각 면의 해상도 (정사각형)
	 */
	void Initialize(ID3D11Device* Device, uint32 InResolution);

	/**
	 * @brief 모든 리소스를 해제합니다.
	 */
	void Release();

	/**
	 * @brief 해상도 변경이 필요한지 확인합니다.
	 * @param NewResolution 새로운 해상도
	 * @return 해상도가 다르면 true
	 */
	bool NeedsResize(uint32 NewResolution) const { return Resolution != NewResolution; }

	/**
	 * @brief 리소스가 유효한지 확인합니다.
	 * @return 모든 리소스가 생성되었으면 true
	 */
	bool IsValid() const
	{
		if (ShadowTextureCube == nullptr || ShadowSRV == nullptr)
			return false;

		for (int i = 0; i < 6; i++)
		{
			if (ShadowDSVs[i] == nullptr)
				return false;
		}

		return true;
	}
};
