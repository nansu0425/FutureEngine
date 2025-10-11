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
    BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());
    SetTexture(UAssetManager::GetInstance().CreateTexture(FName("Asset/Texture/texture.png"), FName("Texture")));
    SetTexture(UAssetManager::GetInstance().CreateTexture(FName("Asset/Texture/texture.png"), FName("FadeTexture")));
}

UDecalComponent::~UDecalComponent()
{
    SafeDelete(BoundingBox);
    SafeDelete(DecalTexture);
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	UpdateFade(DeltaTime);
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

void UDecalComponent::SetFadeTexture(UTexture* InFadeTexture)
{
	if (FadeTexture == InFadeTexture)
	{
		return;
	}

	SafeDelete(FadeTexture);
	FadeTexture = InFadeTexture;
}

UClass* UDecalComponent::GetSpecificWidgetClass() const
{
    return UDecalTextureSelectionWidget::StaticClass();
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
		FadeElapsedTime = 1.0f;;
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
