#include "pch.h"
#include "Actor/Public/DecalSpotLightActor.h"
#include "Component/Public/DecalSpotLightComponent.h"

IMPLEMENT_CLASS(ADecalSpotLightActor, AActor)
ADecalSpotLightActor::ADecalSpotLightActor()
{
}

UClass* ADecalSpotLightActor::GetDefaultRootComponent()
{
	return UDecalSpotLightComponent::StaticClass();
}
