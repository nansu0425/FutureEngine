#include "pch.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Physics/Public/AABB.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"

IMPLEMENT_CLASS(UBillBoardComponent, UPrimitiveComponent)

UBillBoardComponent::UBillBoardComponent()
{
	Type = EPrimitiveType::Sprite;
	
    UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = ResourceManager.GetVertexData(Type);
	VertexBuffer = ResourceManager.GetVertexbuffer(Type);
	NumVertices = ResourceManager.GetNumVertices(Type);

	Indices = ResourceManager.GetIndexData(Type);
	IndexBuffer = ResourceManager.GetIndexBuffer(Type);
	NumIndices = ResourceManager.GetNumIndices(Type);

	RenderState.CullMode = ECullMode::Back;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(Type);

    const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
    if (!TextureCache.empty()) { Sprite = TextureCache.begin()->second; }
}

UBillBoardComponent::~UBillBoardComponent() = default;

void UBillBoardComponent::FaceCamera(const FVector& CameraForward)
{
    FVector Forward = CameraForward;
    FVector Right = Forward.Cross(FVector::UpVector()); Right.Normalize();
    FVector Up = Right.Cross(Forward); Up.Normalize();
    
    // Construct the rotation matrix from the basis vectors
    FMatrix RotationMatrix = FMatrix(Forward, Right, Up);
    
    // Convert the rotation matrix to a quaternion and set the relative rotation
    SetWorldRotation(FQuaternion::FromRotationMatrix(RotationMatrix));
}

UTexture* UBillBoardComponent::GetSprite() const
{
    return Sprite;
}

void UBillBoardComponent::SetSprite(UTexture* InSprite)
{
    Sprite = InSprite;
}

UClass* UBillBoardComponent::GetSpecificWidgetClass() const
{
    return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UBillBoardComponent::GetClassDefaultRenderState()
{
    static FRenderState DefaultRenderState { ECullMode::Back, EFillMode::Solid };
    return DefaultRenderState;
}
