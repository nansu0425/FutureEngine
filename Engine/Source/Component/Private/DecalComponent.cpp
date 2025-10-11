#include "pch.h"
#include "Component/Public/DecalComponent.h"
#include "Physics/Public/OBB.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Render/UI/Widget/Public/DecalTextureSelectionWidget.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
    BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());
    SetTexture(UAssetManager::GetInstance().CreateTexture(FName("Asset/Texture/texture.png"), FName("Texture")));

    // Start with perspective projection by default
    SetPerspective(true);
}

UDecalComponent::~UDecalComponent()
{
    SafeDelete(BoundingBox);
    // DecalTexture is managed by AssetManager, no need to delete here
}

void UDecalComponent::SetTexture(UTexture* InTexture)
{
	if (DecalTexture == InTexture)
	{
		return;
	}
	// SafeDelete(DecalTexture); // Managed by AssetManager
	DecalTexture = InTexture;
}

UClass* UDecalComponent::GetSpecificWidgetClass() const
{
    return UDecalTextureSelectionWidget::StaticClass();
}

void UDecalComponent::SetPerspective(bool bEnable)
{
    bIsPerspective = bEnable;
    UpdateOBB();
    UpdateProjectionMatrix();

}

void UDecalComponent::UpdateProjectionMatrix()
{
    FOBB* Fobb = static_cast<FOBB*>(BoundingBox);

    float W = Fobb->Extents.X;
    float H = Fobb->Extents.Z;

    float FoV = 2 * atan(H / W);
    float AspectRatio = Fobb->Extents.Z / Fobb->Extents.Y;
    float NearClip = 0.1f;
    float FarClip = Fobb->Extents.X * 2;

    float F =  W / H ;

    if (bIsPerspective)
    {
        // Manually calculate the perspective projection matrix

        // Initialize with a clear state
        ProjectionMatrix = FMatrix::Identity();

        // | f/aspect   0        0         0 |
        // |    0       f        0         0 |
        // |    0       0   zf/(zf-zn)     1 |
        // |    0       0  -zn*zf/(zf-zn)  0 |
        ProjectionMatrix.Data[1][1] = F / AspectRatio;
        ProjectionMatrix.Data[2][2] = F;
        ProjectionMatrix.Data[0][0] = FarClip / (FarClip - NearClip);
        ProjectionMatrix.Data[0][3] = -1.0f;
        ProjectionMatrix.Data[3][0] = (-NearClip * FarClip) / (FarClip - NearClip);
        ProjectionMatrix.Data[3][3] = 0.0f;
    }
    else
    {
        ProjectionMatrix = FMatrix::Identity(); // Orthographic decals don't need a projection matrix in this implementation
    }
}

void UDecalComponent::UpdateOBB()
{
    FOBB* OBB = static_cast<FOBB*>(BoundingBox);
   
    // Default OBB for orthographic projection
    OBB->Center = FVector(0.f, 0.f, 0.f);
    OBB->Extents = FVector(0.5f, 0.5f, 0.5f);
    
    // OBB->ScaleRotation is handled by the component's world transform
}
