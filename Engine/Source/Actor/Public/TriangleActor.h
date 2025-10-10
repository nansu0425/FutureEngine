#pragma once
#include "Actor/Public/Actor.h"

class UTriangleComponent;

UCLASS()
class ATriangleActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(ATriangleActor, AActor)

public:
	ATriangleActor();
	virtual ~ATriangleActor() override {}

	
	virtual UClass* GetDefaultRootComponent() override;

private:
	UTriangleComponent* TriangleComponent = nullptr;
};
