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

void UActorComponent::TickComponent(float DeltaTime)

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
	ActorComponent->bIsEditorOnly = bIsEditorOnly;
	ActorComponent->bIsVisualizationComponent = bIsVisualizationComponent;

	return ActorComponent;
}

void UActorComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void UActorComponent::SetIsEditorOnly(bool bInIsEditorOnly)
{
	if (bInIsEditorOnly)
	{
		if (GetOwner()->GetRootComponent() == this)
		{
			// RootComponent는 IsEditorOnly 불가능
			return;
		}
	}
	bIsEditorOnly = bInIsEditorOnly;
}

void UActorComponent::SetIsVisualizationComponent(bool bIsInVisualizationComponent)
{
	bIsVisualizationComponent = bIsInVisualizationComponent;
	if (bIsVisualizationComponent)
	{
		bIsEditorOnly = true;
	}
}
