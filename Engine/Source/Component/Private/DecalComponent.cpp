#include "pch.h"

#include <algorithm>

#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/OBB.h"
#include "Render/UI/Widget/Public/DecalTextureSelectionWidget.h"
#include "Texture/Public/Texture.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
	bOwnsBoundingBox = true;
    BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());

	const TMap<FName, UTexture*>& TextureCache = UAssetManager::GetInstance().GetTextureCache();
	if (!TextureCache.empty()) { SetTexture(TextureCache.begin()->second); }
	SetFadeTexture(UAssetManager::GetInstance().LoadTexture(FName("Data/Texture/PerlinNoiseFadeTexture.png")));
	
    // Start with perspective projection by default
    SetPerspective(true);
    UpdateOBB();
    UpdateProjectionMatrix();
}

UDecalComponent::~UDecalComponent()
{
    SafeDelete(BoundingBox);
    // DecalTexture is managed by AssetManager, no need to delete here
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	UpdateFade(DeltaTime);
}

void UDecalComponent::SetTexture(UTexture* InTexture)
{
	if (DecalTexture == InTexture) { return; }
	DecalTexture = InTexture;
}

void UDecalComponent::SetFadeTexture(UTexture* InFadeTexture)
{
	if (FadeTexture == InFadeTexture)
	{
		return;
	}

	//SafeDelete(FadeTexture);
	FadeTexture = InFadeTexture;
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
		DuplicatedOBB->ScaleRotation = OriginalOBB->ScaleRotation;
	}
	return DuplicatedComponent;
}

void UDecalComponent::SetPerspective(bool bEnable)
{
    if (bIsPerspective != bEnable)
    {
        bIsPerspective = bEnable;
    }
}

void UDecalComponent::UpdateProjectionMatrix()
{
	FMatrix MoveCamera = FMatrix::Identity();
	MoveCamera.Data[3][0] = 0.5f;

	/// 이미 데칼의 로컬 좌표계로 들어온 상태
	/// 기본 OBB는 0.5의 범위(반지름)를 가지기 때문에 x축을 제외한 yz평면을 2배 키워서 NDC에 맞춘다(SRT). 이 후 셰이더에서 uv매핑한다.
	FMatrix Scale = FMatrix::Identity();
	Scale.Data[0][0] = 1.0f;
	Scale.Data[1][1] = 2.0f;
	Scale.Data[2][2] = 2.0f;

	ProjectionMatrix = FMatrix::Identity(); // Orthographic decals don't need a projection matrix in this implementation
	ProjectionMatrix = Scale * MoveCamera * ProjectionMatrix;
}

void UDecalComponent::UpdateOBB()
{
	FOBB* OBB = static_cast<FOBB*>(BoundingBox);
   
	// Default OBB for orthographic projection
	OBB->Center = FVector(0.f, 0.f, 0.f);
	OBB->Extents = FVector(0.5f, 0.5f, 0.5f);
    
	// OBB->ScaleRotation is handled by the component's world transform
}

/*-----------------------------------------------------------------------------
	Decal Fade in/out
 -----------------------------------------------------------------------------*/

void UDecalComponent::BeginFade()
{
	UE_LOG("--- 페이드 아웃 시작 ---");

	if (bIsFading || bIsFadingIn)
	{
		FadeElapsedTime = (FadeProgress * FadeDuration) + FadeStartDelay;	
	}
	else
	{
		FadeElapsedTime = 0.0f;;
	}
	
	bIsFading = true;
	
	bIsFadingIn = false;

	bIsFadePaused = false;
}

void UDecalComponent::BeginFadeIn()
{
	UE_LOG("--- 페이드 인 시작 ---");
	
	if (bIsFading || bIsFadingIn)
	{
		FadeElapsedTime = ((1.0f - FadeProgress) * FadeInDuration) + FadeInStartDelay;
	}
	else
	{
		FadeElapsedTime = 0.0f;
	}
	
	bIsFadingIn = true;
	
	bIsFading = false;
	
	bIsFadePaused = false;
}

void UDecalComponent::StopFade()
{
	bIsFading = false;
	
	bIsFadingIn = false;
	
	FadeProgress = 0.f;
	
	FadeElapsedTime = 0.f;
}

void UDecalComponent::PauseFade()
{
	if (bIsFading || bIsFadingIn)
	{
		bIsFadePaused = true;
	}
}

void UDecalComponent::ResumeFade()
{
	bIsFadePaused = false;
}

void UDecalComponent::UpdateFade(float DeltaTime)
{
	if (bIsFadePaused)
	{
		return;
	}

	if (bIsFading)
	{
		FadeElapsedTime += DeltaTime;
		if (FadeElapsedTime >= FadeStartDelay)
		{
			const float FadeAlpha = (FadeElapsedTime - FadeStartDelay) / FadeDuration;
			FadeProgress = std::clamp(FadeAlpha, 0.0f, 1.0f);

			if (FadeProgress >= 1.0f)
			{
				UE_LOG("--- 페이드 종료 ---");
				bIsFading = false;
				// if(bDestroyOwnerAfterFade)
				// {
				// 	GetOwner()->Destroy();
				// }
			}
		}
	}
	else if (bIsFadingIn)
	{
		FadeElapsedTime += DeltaTime;
		if (FadeElapsedTime >= FadeInStartDelay)
		{
			const float FadeAlpha = (FadeElapsedTime - FadeInStartDelay) / FadeInDuration;
			FadeProgress = 1.f - std::clamp(FadeAlpha, 0.f, 1.f);

			if (FadeProgress <= 0.0f)
			{
				UE_LOG("--- 페이드 종료 ---");
				bIsFadingIn = false;
			}
		}
	}
}
