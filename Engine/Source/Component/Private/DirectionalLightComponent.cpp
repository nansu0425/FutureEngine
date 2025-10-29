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
    LightDirectionArrow.bShouldAlwaysVisible = false;

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
    Icon->SetRelativeScale3D(FVector(5.f,5.f,5.f));
    Icon->SetScreenSizeScaled(true);

    VisualizationIcon = Icon;
    UpdateVisualizationIconTint();
}

void UDirectionalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // PSM Settings
    if (bInIsLoading)
    {
        int32 LoadedMode = ShadowProjectionMode;
        FJsonSerializer::ReadInt32(InOutHandle, "ShadowProjectionMode", LoadedMode);
        ShadowProjectionMode = (uint8)LoadedMode;

        FJsonSerializer::ReadFloat(InOutHandle, "PSMMinInfinityZ", PSMMinInfinityZ);

        // Bool values - read directly from JSON since FJsonSerializer doesn't have ReadBool
        if (InOutHandle.hasKey("PSMUnitCubeClip"))
            bPSMUnitCubeClip = InOutHandle["PSMUnitCubeClip"].ToBool();

        if (InOutHandle.hasKey("PSMSlideBackEnabled"))
            bPSMSlideBackEnabled = InOutHandle["PSMSlideBackEnabled"].ToBool();
    }
    else
    {
        InOutHandle["ShadowProjectionMode"] = (int)ShadowProjectionMode;
        InOutHandle["PSMMinInfinityZ"] = PSMMinInfinityZ;
        InOutHandle["PSMUnitCubeClip"] = bPSMUnitCubeClip;
        InOutHandle["PSMSlideBackEnabled"] = bPSMSlideBackEnabled;
    }
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
    return LightRotation.RotateVector(FVector::ForwardVector());
}

void UDirectionalLightComponent::RenderLightDirectionGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
    if (!InCamera)
    {
        return;
    }

    FVector LightLocation = GetWorldLocation();
    FQuaternion LightRotation = GetWorldRotationAsQuaternion();

    // Gizmo와 동일한 Screen Space Scale 계산
    const FCameraConstants& CameraConstants = InCamera->GetFViewProjConstants();
    const FMatrix& ProjMatrix = CameraConstants.Projection;
    const ECameraType CameraType = InCamera->GetCameraType();
    const float ViewportHeight = InViewport.Height;

    float Scale = 1.0f;
    const float DesiredPixelSize = 60.0f;

    if (ViewportHeight > 1.0f)
    {
        float ProjYY = std::abs(ProjMatrix.Data[1][1]);
        if (ProjYY > 0.0001f)
        {
            if (CameraType == ECameraType::ECT_Perspective)
            {
                const FMatrix& ViewMatrix = CameraConstants.View;
                FVector4 GizmoPos4(LightLocation.X, LightLocation.Y, LightLocation.Z, 1.0f);
                FVector4 ViewSpacePos = GizmoPos4 * ViewMatrix;
                float ProjectedDepth = std::max(std::abs(ViewSpacePos.Z), 1.0f);
                Scale = (DesiredPixelSize * ProjectedDepth) / (ProjYY * ViewportHeight * 0.5f);
            }
            else // Orthographic
            {
                float OrthoHeight = 2.0f / ProjYY;
                Scale = (DesiredPixelSize * OrthoHeight) / ViewportHeight;
            }
            Scale = std::max(0.01f, std::min(Scale, 100.0f));
        }
    }

    LightDirectionArrow.Location = LightLocation;
    LightDirectionArrow.Rotation = LightRotation;
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
