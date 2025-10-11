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

	SafeDelete(FadeTexture);
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
		FadeProgress = 1.0f;;
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
