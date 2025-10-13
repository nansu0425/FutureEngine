#pragma once

UCLASS()
class UHeightFogComponent : USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UHeightFogComponent, USceneComponent)
public:
    UHeightFogComponent();
    virtual ~UHeightFogComponent() override;

    virtual void TickComponent(float DeltaTime) override;
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    UClass* GetSpecificWidgetClass() const override;

    virtual UObject* Duplicate() override;

protected:
    // 안개의 밀도 
    float FogDensity = 1.f;
    // 높이에 따라 안개가 얼마나 빠르게 옅어지는지
    float FogHeightFalloff = 1.f;
    // 카메라로부터 안개가 시작되는 거리
    float StartDistance = 1.f;
    // 안개 효과 계산을 중단하는 최대 거리
    float FogCutoffDistance = 1.f;
    // 안개가 최대로 짙어졌을때 최대 불투명도
    float FogMaxOpacity = 0.5f;

    // 산란되어 들어오는 빛의 색상
    FVector4 FogInscatteringColor = {0.5f, 0.5f, 0.5f, 1.f};
};
