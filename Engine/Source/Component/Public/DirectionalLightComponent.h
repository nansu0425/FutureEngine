#pragma once

#include "LightComponent.h"
#include "Editor/Public/EditorPrimitive.h"

namespace json { class JSON; }

struct FEditorPrimitive;
class UCamera;
class UBillBoardComponent;

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

public:
    UDirectionalLightComponent();
    virtual ~UDirectionalLightComponent() = default;

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
    virtual void BeginPlay() override;
    virtual void EndPlay() override;

    virtual UClass* GetSpecificWidgetClass() const override;

    /*-----------------------------------------------------------------------------
        ULightComponent Features
     -----------------------------------------------------------------------------*/
public:
    virtual ELightComponentType GetLightType() const override { return ELightComponentType::LightType_Directional; }

    /*-----------------------------------------------------------------------------
        UDirectionalLightComponent Features
     -----------------------------------------------------------------------------*/
public:
    FVector GetForwardVector() const;
    void RenderLightDirectionGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);
    FDirectionalLightInfo GetDirectionalLightInfo() const;

    // Shadow mapping
    void SetShadowViewProjection(const FMatrix& ViewProj) { CachedShadowViewProjection = ViewProj; }
    const FMatrix& GetShadowViewProjection() const { return CachedShadowViewProjection; }

private:
    void EnsureVisualizationIcon()override;

private:
    FEditorPrimitive LightDirectionArrow;
    FMatrix CachedShadowViewProjection = FMatrix::Identity();
};

