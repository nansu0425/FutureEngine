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
    uint32 GetSplitNum() const;
    void SetSplitNum(uint32 InSplitNum);

    float GetSplitBlendFactor() const;
    void SetSplitBlendFactor(float InSplitBlendFactor);

    float GetLightViewVolumeZNearBias() const;
    void SetLightViewVolumeZNearBias(float InLightViewVolumeZNearBias);

    float CalculateFrustumXYWithZ(float Z, float Fov);
    FCascadeShadowMapData GetCascadeShadowMapData(
        UCamera* InCamera,
        UDirectionalLightComponent* InDirectionalLight
        );

private:
    uint32 SplitNum = 8;
    float SplitBlendFactor = 0.5f;
    float LightViewVolumeZNearBias = 100.0f;
};