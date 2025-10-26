#pragma once
#include <wrl.h>
#include "Global/Types.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Texture/Public/ShadowMapResources.h"

class FShadowMapPass;

/**
 * @class FShadowMapFilterPass
 * @brief 섀도우 맵에 필터링을 적용하는 렌더 패스입니다.
 *
 * @note 이 클래스는 Variance Shadow Maps (VSM) 사용을 가정하며,
 * Row/Column Compute Shader를 사용한 분리 가능한(separable) 필터(예: 가우시안 블러)를 수행합니다.
 * @todo 여러 개의 연속적인 필터링 작업(파이프라인)을 지원하도록 확장해야 합니다.
 */
class FShadowMapFilterPass : public FRenderPass
{
public:
    FShadowMapFilterPass(FShadowMapPass* InShadowMapPass,
        UPipeline* InPipeline,
        ID3D11ComputeShader* InTextureFilterRowCS,
        ID3D11ComputeShader* InTextureFilterColumnCS
        );

    virtual ~FShadowMapFilterPass();

    void Execute(FRenderingContext& Context) override;
    void Release() override;

    void SetTextureFilterRowCS(ID3D11ComputeShader* InTextureFilterRowCS) { TextureFilterRowCS = InTextureFilterRowCS; }

    void SetTextureFilterColumnCS(ID3D11ComputeShader* InTextureFilterColumnCS) { TextureFilterColumnCS = InTextureFilterColumnCS; }

    // TODO: Shadow Map 해상도에 따라 동적으로 내부 임시버퍼 크기조절 필요
    void OnResize() {}

private:
    /**
     * @brief 섀도우 맵에 분리 가능한(separable) 필터링을 적용합니다.
     * @details 이 함수는 컴퓨트 셰이더를 사용하여 2-pass (가로/세로) 필터링을 수행합니다.
     *
     * 1. 가로(Row) 패스: 원본 섀도우 맵(SRV) -> 임시 텍스처(UAV)
     * 2. 세로(Column) 패스: 임시 텍스처(SRV) -> 원본 섀도우 맵(UAV)
     *
     * @note 주로 박스(Box) 필터나 가우시안(Gaussian) 블러 같은 일반적인 분리형 필터에 사용됩니다.
     * @todo 미구현 기능
     */
    void FilterShadowMap(const FShadowMapResource* ShadowMap) const;
    
    /**
     * @brief 섀도우 맵에 Summed Area Table (SAT) 기반 필터링을 수행합니다.
     */
    void SummedAreaFilterShadowMap(const FShadowMapResource* ShadowMap) const;

    // 필터링 패스에서 사용하는 임시 텍스처의 고정 너비
    // @note 필터링할 섀도우 맵의 실제 너비는 이 값(1024)보다 작거나 같아야 정상 작동합니다.
    static constexpr uint32 TEXTURE_WIDTH = 1024;

    // 필터링 패스에서 사용하는 **임시 텍스처**의 고정 높이
    // @note 필터링할 섀도우 맵의 실제 높이는 이 값(1024)보다 작거나 같아야 정상 작동합니다.
    static constexpr uint32 TEXTURE_HEIGHT = 1024;

    static constexpr uint32 THREAD_BLOCK_SIZE_X = 16;
    
    static constexpr uint32 THREAD_BLOCK_SIZE_Y = 16;

    // Shadow map pass
    FShadowMapPass* ShadowMapPass;
    
    // Shaders
    ID3D11ComputeShader* TextureFilterRowCS;
    ID3D11ComputeShader* TextureFilterColumnCS;
    
    // Constant buffers
    struct FTextureInfo
    {
        uint32 TextureWidth;
        uint32 TextureHeight;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBufferTextureInfo = nullptr;

    // Double Buffering용 임시 버퍼
    // @note R32G32 format만 지원 (VSM의 1차 모멘트, 2차 모멘트 저장용)
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> TemporaryTexture;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> TemporaryUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TemporarySRV;
};
