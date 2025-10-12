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
    UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = ResourceManager.GetVertexData(EPrimitiveType::Sprite);
	VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Sprite);
	NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Sprite);

	Indices = ResourceManager.GetIndexData(EPrimitiveType::Sprite);
	IndexBuffer = ResourceManager.GetIndexBuffer(EPrimitiveType::Sprite);
	NumIndices = ResourceManager.GetNumIndices(EPrimitiveType::Sprite);

	RenderState.CullMode = ECullMode::Back;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(EPrimitiveType::Sprite);

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
