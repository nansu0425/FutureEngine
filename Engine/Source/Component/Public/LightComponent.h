#pragma once

#include "SceneComponent.h"
#include "LightComponentBase.h"
#include "Component/Public/EditorIconComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

class UEditorIconComponent;

UENUM()
enum class ELightComponentType
{
    LightType_Directional = 0,
    LightType_Point       = 1,
    LightType_Spot        = 2,
    LightType_Ambient     = 3,
    LightType_Rect        = 4,
    LightType_Max         = 5
};
DECLARE_ENUM_REFLECTION(ELightComponentType)

UCLASS()
class ULightComponent : public ULightComponentBase
{
    GENERATED_BODY()
    DECLARE_CLASS(ULightComponent, ULightComponentBase)

public:
    ULightComponent() = default;

    virtual ~ULightComponent() = default;
    
    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;
        
    virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override { Super::BeginPlay(); }
    
    virtual void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }

    virtual void EndPlay() override { Super::EndPlay(); }

    /*-----------------------------------------------------------------------------
        ULightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    virtual ELightComponentType GetLightType() const { return ELightComponentType::LightType_Max; }

    // --- [UE Style] ---

    // virtual FBox GetBoundingBox() const;

    // virtual FSphere GetBoundingSphere() const;
    
    /** @note Sets the light intensity and clamps it to the same range as Unreal Engine (0.0 - 20.0). */

    void SetIntensity(float InIntensity) override;
    void SetLightColor(FVector InLightColor) override;

    virtual void EnsureVisualizationIcon(){};

    UEditorIconComponent* GetEditorIconComponent() const
    {
        return VisualizationIcon;
    }

    void SetEditorIconComponent(UEditorIconComponent* InEditorIconComponent)
    {
        VisualizationIcon = InEditorIconComponent;
    }

    void RefreshVisualizationIconBinding();

    /*-----------------------------------------------------------------------------
        Shadow Quality Parameters
     -----------------------------------------------------------------------------*/
public:
    /** @brief Shadow map 해상도 배율을 반환합니다 (1.0 = 기본 1024x1024). */
    float GetShadowResolutionScale() const { return ShadowResolutionScale; }

    /** @brief Shadow map 해상도 배율을 설정합니다 (0.25 ~ 4.0). */
    void SetShadowResolutionScale(float InScale) { ShadowResolutionScale = std::clamp(InScale, 0.25f, 4.0f); }

    /** @brief Shadow acne 방지를 위한 depth bias를 반환합니다. */
    float GetShadowBias() const { return ShadowBias; }

    /** @brief Shadow acne 방지를 위한 depth bias를 설정합니다 (0.0 ~ 0.1). */
    void SetShadowBias(float InBias) {ShadowBias = std::clamp(InBias, 0.0f, 0.1f); }

    /** @brief 표면 기울기에 따른 bias를 반환합니다 (Peter panning 방지). */
    float GetShadowSlopeBias() const { return ShadowSlopeBias; }

    /** @brief 표면 기울기에 따른 bias를 설정합니다 (0.0 ~ 4.0). */
    void SetShadowSlopeBias(float InSlopeBias) { ShadowSlopeBias = std::clamp(InSlopeBias, 0.0f, 4.0f); }

    /** @brief Shadow 가장자리 sharpness를 반환합니다 (0=soft, 1=sharp). */
    float GetShadowSharpen() const { return ShadowSharpen; }

    /** @brief Shadow 가장자리 sharpness를 설정합니다 (0.0 ~ 1.0). */
    void SetShadowSharpen(float InSharpen) { ShadowSharpen = std::clamp(InSharpen, 0.0f, 1.0f); }

    /** @brief 실제 shadow map 해상도를 계산하여 반환합니다 (기본 1024 * scale). */
    uint32 GetShadowMapResolution() const { return static_cast<uint32>(1024.0f * ShadowResolutionScale); }

protected:
    void UpdateVisualizationIconTint();

    UEditorIconComponent* VisualizationIcon = nullptr;

    /** Shadow map 해상도 배율 (0.25 ~ 4.0)
     * 0.25 = 256x256, 0.5 = 512x512, 1.0 = 1024x1024, 2.0 = 2048x2048, 4.0 = 4096x4096 */
    float ShadowResolutionScale = 1.0f;

    /** Depth bias for shadow acne prevention (0.0 ~ 0.1) */
    float ShadowBias = 0.005f;

    /** Slope-scale depth bias (0.0 ~ 4.0) */
    float ShadowSlopeBias = 1.0f;

    /** Shadow sharpness/softness (0.0 = very soft, 1.0 = sharp) */
    float ShadowSharpen = 0.5f;

private:

};
