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

    FMatrix GetProjection()const;
    int IsPerspective()const;



protected:
    class UTexture* DecalTexture = nullptr;

    FMatrix Projection;
    int IsPersp;
};
