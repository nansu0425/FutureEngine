#pragma once
#include "Widget.h"

class UPrimitiveSpawnWidget
	:public UWidget
{
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	void SpawnActors();

	// Special Member Function
	UPrimitiveSpawnWidget();
	~UPrimitiveSpawnWidget() override;

private:
	void LoadActorClasses();

	TArray<FString> ActorClassNames;
	TArray<UClass*> ActorClasses;
	FName SelectedActorClassName = FName::None;
	int32 SelectedActorClassIndex = 0;
	int32 NumberOfSpawn = 1;
	float SpawnRangeMin = -10.0f;
	float SpawnRangeMax = 10.0f;
};
