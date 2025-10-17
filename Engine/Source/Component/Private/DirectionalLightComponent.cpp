#include "pch.h"

#include "Component/Public/DirectionalLightComponent.h"
//#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/ActorComponent.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
    UAssetManager& ResourceManager = UAssetManager::GetInstance();
    
    // 화살표 프리미티브 설정 (빛 방향 표시용)
    LightDirectionArrow.VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
    LightDirectionArrow.NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
    LightDirectionArrow.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    LightDirectionArrow.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 흰색
    LightDirectionArrow.bShouldAlwaysVisible = true;



    //UBillBoardComponent* Billboard = CreateDefaultSubobject<UBillBoardComponent>();
    //Billboard->AttachToComponent(GetRootComponent());
    //Billboard->SetIsVisualizationComponent(true);
    //Billboard->SetSprite(UAssetManager::GetInstance().LoadTexture("Data/Icons/PointLight_64x.png"));
    //Billboard->SetScreenSizeScaled(true);
}

void UDirectionalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        
    }
    else
    {
       
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
    //return UPointLightComponentWidget::StaticClass();
    return nullptr;
}

void UDirectionalLightComponent::RenderLightDirectionGizmo(UCamera* InCamera)
{
    if (!InCamera) return;
    
    // 라이트의 위치와 회전 가져오기
    FVector LightLocation = GetWorldLocation();
    FQuaternion LightRotation = GetWorldRotationAsQuaternion();
    
    // 카메라 거리 기반 스케일 조정 (기즈모처럼)
    float Distance = (InCamera->GetLocation() - LightLocation).Length();
    float Scale = Distance * 0.2f; // 적절한 크기
    if (Distance < 7.0f) Scale = 7.0f * 0.2f; // 최소 크기 보장
    
    // DirectionalLight는 -Z축 방향으로 빛이 나감
    // 화살표는 기본적으로 +X축을 향하므로, +X -> -Z로 회전 필요
    // +X를 -Z로 변환: Y축 기준 -90도 회전
    FQuaternion ArrowToNegZ = FQuaternion::FromAxisAngle(FVector::RightVector(), -90.0f * (PI / 180.0f));
    
    // 최종 회전 = 라이트 회전 * 화살표 보정 회전
    FQuaternion FinalRotation = ArrowToNegZ * LightRotation;
    
    // 화살표 프리미티브 업데이트
    LightDirectionArrow.Location = LightLocation;
    LightDirectionArrow.Rotation = FinalRotation;
    LightDirectionArrow.Scale = FVector(Scale, Scale, Scale);
    
    // 렌더링
    FRenderState RenderState;
    RenderState.FillMode = EFillMode::Solid;
    RenderState.CullMode = ECullMode::None;
    
    URenderer::GetInstance().RenderEditorPrimitive(LightDirectionArrow, RenderState);
}
