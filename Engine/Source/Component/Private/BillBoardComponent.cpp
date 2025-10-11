#include "pch.h"
#include "Component/Public/BillBoardComponent.h""
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
	IndexBuffer = ResourceManager.GetIndexbuffer(Type);
	NumIndices = ResourceManager.GetNumIndices(Type);

	RenderState.CullMode = ECullMode::Back;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(Type);

    Sampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
    if (!Sampler)
    {
        assert(false);
    }

    const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
        UAssetManager::GetInstance().GetTextureCache();
    if (!TextureCache.empty())
        Sprite = *TextureCache.begin();
}

UBillBoardComponent::~UBillBoardComponent()
{
    if (Sampler)
        Sampler->Release();
}

void UBillBoardComponent::FaceCamera(
    const FVector& CameraPosition,
    const FVector& CameraUp,
    const FVector& FallbackUp
)
{
    // Front vector points from the billboard to the camera
    FVector Front = (CameraPosition - GetRelativeLocation());
    Front.Normalize();

    // Right vector is perpendicular to the Front and world's Up vector
    FVector Right = CameraUp.Cross(Front);
    if (Right.Length() <= 0.0001f)
    {
        // Use a fallback if the camera is looking straight down or up
        Right = FallbackUp.Cross(Front);
    }
    Right.Normalize();

    // Up vector is perpendicular to the Right and Front vectors
    FVector Up = Front.Cross(Right);
    Up.Normalize();

    // Construct the rotation matrix from the basis vectors
    FMatrix RotationMatrix = FMatrix(Right, Up, Front);

    // Convert the rotation matrix to a quaternion and set the relative rotation
    SetRelativeRotation(FQuaternion::FromRotationMatrix(RotationMatrix));
}

const TPair<FName, ID3D11ShaderResourceView*>& UBillBoardComponent::GetSprite() const
{
    return Sprite;
}

void UBillBoardComponent::SetSprite(const TPair<FName, ID3D11ShaderResourceView*>& InSprite)
{
    Sprite = InSprite;
}

ID3D11SamplerState* UBillBoardComponent::GetSampler() const
{ 
    return Sampler;
};

UClass* UBillBoardComponent::GetSpecificWidgetClass() const
{
    return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UBillBoardComponent::GetClassDefaultRenderState()
{
    static FRenderState DefaultRenderState { ECullMode::Back, EFillMode::Solid };
    return DefaultRenderState;
}
