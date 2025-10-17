#pragma once
#include "Global/Constant.h"
#include "LightComponent.h"

UCLASS()
class USpotLightComponent : public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponent, ULightComponent)

public:
    USpotLightComponent() = default;

    virtual ~USpotLightComponent() = default;
    
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

    virtual UClass* GetSpecificWidgetClass() const override; 

    /*-----------------------------------------------------------------------------
        ULightComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Spot; }

    /*-----------------------------------------------------------------------------
        USpotLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    // --- Getters & Setters ---

    float GetDistanceFalloffExponent() const { return DistanceFalloffExponent; }
    float GetAngleFalloffExponent() const { return AngleFalloffExponent; }
    float GetAttenuationRadius() const { return AttenuationRadius; }
    float GetAttenuationAngle() const { return AttenuationAngleRad; }
    
    /** @note Sets the light falloff exponent and clamps it to the same range as Unreal Engine (2.0 - 16.0). */
    void SetDistanceFalloffExponent(float InDistanceFalloffExponent) { DistanceFalloffExponent = std::clamp(InDistanceFalloffExponent, 2.0f, 16.0f); }
    void SetAngleFalloffExponent(float InAngleFlloffExponent) { AngleFalloffExponent = std::clamp(InAngleFlloffExponent, 1.0f, 128.0f); }
    void SetAttenuationRadius(float InAttenuationRadius) { AttenuationRadius = InAttenuationRadius; }
    void SetAttenuationAngle(float InAttenuationAngle) { AttenuationAngleRad = InAttenuationAngle; }

private:
    /**
     * 작을수록 attenuation이 선형에 가깝게 완만하게 감소하고,
     * 커질수록 중심 인근에서는 거의 감소하지 않다가 가장자리 부근에서 급격히 감소합니다.
     */
    float DistanceFalloffExponent = 2.0f; // clamp(1 - (d/R)^(DistFalloffExponent), 0, 1)
    float AngleFalloffExponent = 64.0f; // clamp(cos(theta) ^ (AngleFalloffExponent), 0, 1)
    
    /** Radius and angle of light source shape. */
    float AttenuationRadius = 10.0f;
    float AttenuationAngleRad = PI / 4.0f;
};