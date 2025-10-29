#pragma once
#include "Global/Constant.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/EditorPrimitive.h"

struct FEditorPrimitive;
class UCamera;


UCLASS()
class USpotLightComponent : public UPointLightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponent, UPointLightComponent)

public:
    USpotLightComponent();

    virtual ~USpotLightComponent() = default;
    
    /*-----------------------------------------------------------------------------
        UObject Features
     -----------------------------------------------------------------------------*/
public:
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    virtual UObject* Duplicate() override;

    /*-----------------------------------------------------------------------------
        UActorComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual void BeginPlay() override { Super::BeginPlay(); }
    
    virtual void TickComponent(float DeltaTime) override { Super::TickComponent(DeltaTime); }

    virtual void EndPlay() override { Super::EndPlay(); }

    virtual UClass* GetSpecificWidgetClass() const override; 

    /*-----------------------------------------------------------------------------
        UPointLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Spot; }

    /*-----------------------------------------------------------------------------
        USpotLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    // 월드 좌표계에서의 forward vector를 반환합니다.
    FVector GetForwardVector() const;
    
    float GetAngleFalloffExponent() const { return AngleFalloffExponent; }
    float GetOuterConeAngle() const { return OuterConeAngleRad; }
    float GetInnerConeAngle() const { return InnerConeAngleRad; }
    
    void SetAngleFalloffExponent(float const InAngleFalloffExponent) { AngleFalloffExponent = std::clamp(InAngleFalloffExponent, 1.0f, 128.0f); }
    void SetOuterAngle(float const InAttenuationAngleRad);
    void SetInnerAngle(float const InAttenuationAngleRad);

    void RenderLightDirectionGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);

    FSpotLightInfo GetSpotLightInfo() const;

    // Shadow mapping
    void SetShadowViewProjection(const FMatrix& ViewProj) { CachedShadowViewProjection = ViewProj; }
    const FMatrix& GetShadowViewProjection() const { return CachedShadowViewProjection; }

private:
    void EnsureVisualizationIcon()override;

private:
    /**
     * 거리 감쇠 지수는 부모(UPointLightComponent)가 관리하며,
     * 각도 감쇠 지수는 이 값을 통해 제어합니다.
     */
    float AngleFalloffExponent = 2.0f;

    /** Angle of light source shape. */
    float OuterConeAngleRad = PI / 4.0f;
    float InnerConeAngleRad = 0.0f;

    // Shadow mapping
    FMatrix CachedShadowViewProjection = FMatrix::Identity();

    // 라이트 방향을 시각화하는 화살표 프리미티브
    FEditorPrimitive LightDirectionArrow;
};
