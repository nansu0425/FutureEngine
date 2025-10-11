#pragma once
#include "Component/Public/PrimitiveComponent.h"

UCLASS()
class UDecalComponent : public UPrimitiveComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

public:
    UDecalComponent();
    ~UDecalComponent();

    void SetTexture(class UTexture* InTexture);
    class UTexture* GetTexture() const { return DecalTexture; }

    const TPair<FName, ID3D11ShaderResourceView*>& GetSprite() const;
    UClass* GetSpecificWidgetClass() const override;

    // --- Perspective Projection ---
    void SetPerspective(bool bEnable);
    void SetFOV(float InFOV) { FOV = InFOV; UpdateProjectionMatrix(); UpdateOBB(); }
    void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; UpdateProjectionMatrix(); UpdateOBB(); }
    void SetClipDistances(float InNear, float InFar) { NearClip = InNear; FarClip = InFar; UpdateProjectionMatrix(); UpdateOBB(); }

    FMatrix GetProjectionMatrix() const { return ProjectionMatrix; }
    bool IsPerspective() const { return bIsPerspective; }

    void UpdateProjectionMatrix();
protected:

    void UpdateOBB();

protected:
    class UTexture* DecalTexture = nullptr;

public:
	virtual UObject* Duplicate() override;

    // --- Projection Properties ---
    FMatrix ProjectionMatrix;
    bool bIsPerspective = false;

    float FOV; // in Degrees
    float AspectRatio;
    float NearClip;
    float FarClip;
};
