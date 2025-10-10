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

    void SetTexture(class UTexture* InTexture) { DecalTexture = InTexture; }
    class UTexture* GetTexture() const { return DecalTexture; }

protected:
    class UTexture* DecalTexture = nullptr;
};
