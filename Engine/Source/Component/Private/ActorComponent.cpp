#include "pch.h"
#include "Component/Public/ActorComponent.h"

IMPLEMENT_CLASS(UActorComponent, UObject)

UActorComponent::UActorComponent()
{
	ComponentType = EComponentType::Actor;
}

UActorComponent::~UActorComponent()
{
	SetOuter(nullptr);
}

void UActorComponent::BeginPlay()
{

}

void UActorComponent::TickComponent()
{

}

void UActorComponent::EndPlay()
{

}


void UActorComponent::OnSelected()
{

}


void UActorComponent::OnDeselected()
{

}

UObject* UActorComponent::Duplicate()
{
	UActorComponent* ActorComponent = Cast<UActorComponent>(Super::Duplicate());
	ActorComponent->bCanEverTick = bCanEverTick;
	ActorComponent->ComponentType = ComponentType;

	return ActorComponent;
}

void UActorComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}
