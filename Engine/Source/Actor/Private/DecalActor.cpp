#include "pch.h"
#include "Actor/Public/DecalActor.h"

IMPLEMENT_CLASS(ADecalActor, AActor)
ADecalActor::ADecalActor()
{
}

UClass* ADecalActor::GetDefaultRootComponent()
{
    return UDecalComponent::StaticClass();
}
