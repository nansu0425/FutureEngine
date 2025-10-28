#include "pch.h"

#include "Component/Public/DirectionalLightComponent.h"
#include "Render/UI/Widget/Public/DirectionalLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Component/Public/ActorComponent.h"
#include "Editor/Public/EditorEngine.h"
#include "Actor/Public/Actor.h"
#include "Level/Public/World.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
    Intensity = 3.0f;

    UAssetManager& ResourceManager = UAssetManager::GetInstance();

    // 화살표 프리미티브 설정 (빛 방향 표시용)
    LightDirectionArrow.VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
    LightDirectionArrow.NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
    LightDirectionArrow.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    LightDirectionArrow.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    LightDirectionArrow.bShouldAlwaysVisible = true;

    //EnsureVisualizationIcon();
}

void UDirectionalLightComponent::BeginPlay()
{
    Super::BeginPlay();
    
}

void UDirectionalLightComponent::EndPlay()
{
    Super::EndPlay();
    VisualizationIcon = nullptr;
}

void UDirectionalLightComponent::EnsureVisualizationIcon()
{
    if (VisualizationIcon)
    {
        return;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    if (GWorld)
    {
        EWorldType WorldType = GWorld->GetWorldType();
        if (WorldType != EWorldType::Editor && WorldType != EWorldType::EditorPreview)
        {
            return;
        }
    }

    UEditorIconComponent* Icon = OwnerActor->AddComponent<UEditorIconComponent>();
    if (!Icon)
    {
        return;
    }
    Icon->AttachToComponent(this);
    Icon->SetIsVisualizationComponent(true);
    Icon->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/S_LightDirectional.png"));
    Icon->SetRelativeScale3D(FVector(2.f,2.f,2.f));
    Icon->SetScreenSizeScaled(true);

    VisualizationIcon = Icon;
    UpdateVisualizationIconTint();
}

void UDirectionalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    
}

UObject* UDirectionalLightComponent::Duplicate()
{
    UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>(Super::Duplicate());
    return DirectionalLightComponent;
}

void UDirectionalLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
    Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UDirectionalLightComponent::GetSpecificWidgetClass() const
{
    return UDirectionalLightComponentWidget::StaticClass();
}

FVector UDirectionalLightComponent::GetForwardVector() const
{
    FQuaternion LightRotation = GetWorldRotationAsQuaternion();

    FQuaternion ArrowToNegZ = FQuaternion::FromAxisAngle(FVector::RightVector(), -90.0f * (PI / 180.0f));
    FQuaternion FinalRotation = ArrowToNegZ * LightRotation;

    return FinalRotation.RotateVector(FVector::ForwardVector());
}

void UDirectionalLightComponent::RenderLightDirectionGizmo(UCamera* InCamera)
{
    if (!InCamera) return;

    FVector LightLocation = GetWorldLocation();
    FQuaternion LightRotation = GetWorldRotationAsQuaternion();

    float Distance = (InCamera->GetLocation() - LightLocation).Length();
    float Scale = Distance * 0.2f;
    if (Distance < 7.0f) Scale = 7.0f * 0.2f;

    FQuaternion ArrowToNegZ = FQuaternion::FromAxisAngle(FVector::RightVector(), -90.0f * (PI / 180.0f));
    FQuaternion FinalRotation = ArrowToNegZ * LightRotation;

    LightDirectionArrow.Location = LightLocation;
    LightDirectionArrow.Rotation = FinalRotation;
    LightDirectionArrow.Scale = FVector(Scale, Scale, Scale);

    FRenderState RenderState;
    RenderState.FillMode = EFillMode::Solid;
    RenderState.CullMode = ECullMode::None;

    URenderer::GetInstance().RenderEditorPrimitive(LightDirectionArrow, RenderState);
}

FDirectionalLightInfo UDirectionalLightComponent::GetDirectionalLightInfo() const
{
    FDirectionalLightInfo Info;
    Info.Color = FVector4(LightColor, 1);
    Info.Direction = GetForwardVector();
    Info.Intensity = Intensity;

    // Shadow parameters
    // Info.LightViewProjection = CachedShadowViewProjection; // Updated by ShadowMapPass
    Info.CastShadow = GetCastShadows() ? 1u : 0u;
    Info.ShadowModeIndex = static_cast<uint32>(GetShadowModeIndex());
    Info.ShadowBias = GetShadowBias();
    Info.ShadowSlopeBias = GetShadowSlopeBias();
    Info.ShadowSharpen = GetShadowSharpen();
    Info.Resolution = GetShadowResolutionScale();

    return Info;
}
