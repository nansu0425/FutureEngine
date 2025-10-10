#include "pch.h"
#include "Actor/Public/CubeActor.h"
#include "Component/Mesh/Public/CubeComponent.h"

IMPLEMENT_CLASS(ACubeActor, AActor)

ACubeActor::ACubeActor()
{
}

UClass* ACubeActor::GetDefaultRootComponent()
{
    return UCubeComponent::StaticClass();
}
