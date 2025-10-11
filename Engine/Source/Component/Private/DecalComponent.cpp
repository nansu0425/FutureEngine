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


	const float FovY = 60.0f;
	const float RadianFovY = FVector::GetDegreeToRadian(FovY);
	const float F = 1.0f / std::tanf(RadianFovY * 0.5f);
	const float Aspect = static_cast<FOBB*>(BoundingBox)->GetExtents().X / static_cast<FOBB*>(BoundingBox)->GetExtents().Y;  // 여기에 obb의 가로세로가 들어가야함
	const float	NearZ = 0.1f;
	const float FarZ = 1000.0f;
	Projection = FMatrix::Identity();
	FMatrix Projection = FMatrix::Identity();
	// | f/aspect   0        0         0 |
	// |    0       f        0         0 |
	// |    0       0   zf/(zf-zn)     1 |
	// |    0       0  -zn*zf/(zf-zn)  0 |
	Projection.Data[0][0] = F / Aspect;
	Projection.Data[1][1] = F;
	Projection.Data[2][2] = FarZ / (FarZ - NearZ);
	Projection.Data[2][3] = 1.0f;
	Projection.Data[3][2] = (-NearZ * FarZ) / (FarZ - NearZ);
	Projection.Data[3][3] = 0.0f;



	IsPersp = 1; // 기본 직교투영
}

UDecalComponent::~UDecalComponent()
{
    SafeDelete(BoundingBox);
    SafeDelete(DecalTexture);
}

void UDecalComponent::SetTexture(UTexture* InTexture)
{
	if (DecalTexture == InTexture)
	{
		return;
	}

	SafeDelete(DecalTexture);
	DecalTexture = InTexture;
}

UClass* UDecalComponent::GetSpecificWidgetClass() const
{
    return UDecalTextureSelectionWidget::StaticClass();
}

FMatrix UDecalComponent::GetProjection() const
{
	return Projection;
}

int UDecalComponent::IsPerspective() const
{
	return IsPersp;
}
