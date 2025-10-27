#pragma once

#include <wrl.h>

#include "Component/Public/PointLightComponent.h"
#include "Global/Types.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Texture/Public/ShadowMapResources.h"
#include "Texture/Public/TextureFilter.h"

class FShadowMapPass;
class ULightComponentBase;

/**
 * @class FShadowMapFilterPass
 * @brief 섀도우 맵에 필터링을 적용하는 렌더 패스
 *
 * @note 이 클래스는 Variance Shadow Maps (VSM) 사용을 가정하며,
 * Row/Column Compute Shader를 사용한 분리 가능한(separable) 필터(예: 가우시안 블러)를 수행한다.
 */
class FShadowMapFilterPass : public FRenderPass
{
public:
    FShadowMapFilterPass(FShadowMapPass* InShadowMapPass, UPipeline* InPipeline);

    virtual ~FShadowMapFilterPass();

    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    /**
     * @brief 섀도우 맵에 분리 가능한(separable) 필터링을 적용한다.
     * @details 이 함수는 컴퓨트 셰이더를 사용하여 2-pass (가로/세로) 필터링을 수행한다.
     *
     * 1. 가로(Row) 패스: 원본 섀도우 맵(SRV) -> 임시 텍스처(UAV)
     * 2. 세로(Column) 패스: 임시 텍스처(SRV) -> 원본 섀도우 맵(UAV)
     */
    void FilterShadowMap(const ULightComponentBase* LightComponent, const FShadowMapResource* ShadowMap);

    /**
     * @brief 아틀라스 섀도우 맵에 분리 가능한(separable) 필터링을 적용한다.
     * @details 이 함수는 컴퓨트 셰이더를 사용하여 2-pass (가로/세로) 필터링을 수행한다.
     *
     * 1. 가로(Row) 패스: 원본 섀도우 맵(SRV) -> 임시 텍스처(UAV)
     * 2. 세로(Column) 패스: 임시 텍스처(SRV) -> 원본 섀도우 맵(UAV)
     */
    void FilterShadowAtlasMap(const ULightComponentBase* LightComponent, const FShadowMapResource* ShadowMap, uint32 RegionStartX, uint32 RegionStartY, uint32 RegionWidth, uint32 RegionHeight);

    // @todo: 쉐도우 맵 크기 동적조정 기능 추가전까지 사용
    static constexpr uint32 TEXTURE_WIDTH = 1024;
    
    static constexpr uint32 TEXTURE_HEIGHT = 1024;
    
    // 컴퓨트 셰이더의 스레드 그룹 차원(X) 크기.
    // @note HLSL의 [numthreads(X, Y, Z)] 지시자에서 X 값과 반드시 일치해야 한다.
    static constexpr uint32 THREAD_BLOCK_SIZE_X = 16;

    // 컴퓨트 셰이더의 스레드 그룹 차원(Y) 크기.
    // @note HLSL의 [numthreads(X, Y, Z)] 지시자에서 Y 값과 반드시 일치해야 한다.
    static constexpr uint32 THREAD_BLOCK_SIZE_Y = 16;

    FShadowMapPass* ShadowMapPass;

    TMap<EShadowModeIndex, std::unique_ptr<FTextureFilter>> TextureFilterMap;
};
