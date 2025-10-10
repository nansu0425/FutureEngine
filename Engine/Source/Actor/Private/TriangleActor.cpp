#include "pch.h"
#include "Actor/Public/TriangleActor.h"
#include "Component/Mesh/Public/TriangleComponent.h"

IMPLEMENT_CLASS(ATriangleActor, AActor)

ATriangleActor::ATriangleActor()
{
}

UClass* ATriangleActor::GetDefaultRootComponent()
{
    return UTriangleComponent::StaticClass();
}
