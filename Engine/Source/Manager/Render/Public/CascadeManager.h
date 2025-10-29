#pragma once

#include "Component/Public/DirectionalLightComponent.h"
#include "Render/RenderPass/Public/ShadowData.h"
#include "Editor/Public/Camera.h"

UCLASS()
class UCascadeManager : public UObject
{
    GENERATED_BODY()
    DECLARE_SINGLETON_CLASS(UCascadeManager, UObject)

public:
    int32 GetSplitNum() const;
    void SetSplitNum(int32 InSplitNum);

    float GetSplitBlendFactor() const;
    void SetSplitBlendFactor(float InSplitBlendFactor);

    float GetLightViewVolumeZNearBias() const;
    void SetLightViewVolumeZNearBias(float InLightViewVolumeZNearBias);

    float GetBandingAreaFactor() const;
    void SetBandingAreaFactor(float InBandingAreaFactor);

    float CalculateFrustumXYWithZ(float Z, float Fov);
    FCascadeShadowMapData GetCascadeShadowMapData(
        UCamera* InCamera,
        UDirectionalLightComponent* InDirectionalLight
        );

    static constexpr int32 SPLIT_NUM_MIN = 1;
    static constexpr int32 SPLIT_NUM_MAX = 8;

    static constexpr float SPLIT_BLEND_FACTOR_MIN = 0.0f;
    static constexpr float SPLIT_BLEND_FACTOR_MAX = 1.0f;

    static constexpr float LIGHT_VIEW_VOLUME_ZNEAR_BIAS_MIN = 0.0f;
    static constexpr float LIGHT_VIEW_VOLUME_ZNEAR_BIAS_MAX = 1000.0f;

    static constexpr float BANDING_AREA_FACTOR_MIN = 1.0f;
    static constexpr float BANDING_AREA_FACTOR_MAX = 1.5f;

private:
    int32 SplitNum = 8;
    float SplitBlendFactor = 0.5f;
    float LightViewVolumeZNearBias = 100.0f;
    float BandingAreaFactor = 1.1f;
};