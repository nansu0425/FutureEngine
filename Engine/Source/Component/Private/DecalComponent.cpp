#include "pch.h"
#include "Component/Public/DecalComponent.h"
#include "Physics/Public/OBB.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Render/UI/Widget/Public/DecalTextureSelectionWidget.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
	bOwnsBoundingBox = true;
    BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());
	SetTexture(UAssetManager::GetInstance().CreateTexture(FName("Asset/Texture/texture.png"), FName("Texture")));
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

UObject* UDecalComponent::Duplicate()
{
	UDecalComponent* DuplicatedComponent = Cast<UDecalComponent>(Super::Duplicate());

	FOBB* OriginalOBB = static_cast<FOBB*>(BoundingBox);
	FOBB* DuplicatedOBB = static_cast<FOBB*>(DuplicatedComponent->BoundingBox);
	if (OriginalOBB && DuplicatedOBB)
	{
		DuplicatedOBB->Center = OriginalOBB->Center;
		DuplicatedOBB->Extents = OriginalOBB->Extents;
		DuplicatedOBB->Rotation = OriginalOBB->Rotation;
		DuplicatedOBB->ScaleRotation = OriginalOBB->ScaleRotation;
	}

	return DuplicatedComponent;
}