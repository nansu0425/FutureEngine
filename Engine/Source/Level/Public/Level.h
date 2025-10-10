#pragma once
#include "Core/Public/Object.h"
#include "Editor/Public/Camera.h"
#include "Global/Enum.h"

namespace json { class JSON; }
using JSON = json::JSON;

class UWorld;
class AActor;
class UPrimitiveComponent;
class FOctree;

UCLASS()
class ULevel :
	public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(ULevel, UObject)
public:
	ULevel();
	ULevel(const FName& InName);
	~ULevel() override;

	virtual void Init();

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	const TArray<AActor*>& GetLevelActors() const { return LevelActors; }

	void AddLevelPrimitiveComponent(AActor* Actor);

	void RegisterPrimitiveComponent(UPrimitiveComponent* InComponent);
	void UnregisterPrimitiveComponent(UPrimitiveComponent* InComponent);
	bool DestroyActor(AActor* InActor);

	uint64 GetShowFlags() const { return ShowFlags; }
	void SetShowFlags(uint64 InShowFlags) { ShowFlags = InShowFlags; }

	void UpdatePrimitiveInOctree(UPrimitiveComponent* InComponent);

	FOctree* GetStaticOctree() { return StaticOctree; }
	TArray<UPrimitiveComponent*>& GetDynamicPrimitives() { return DynamicPrimitives; }

	friend class UWorld;
public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;

private:
	AActor* SpawnActorToLevel(UClass* InActorClass, const FName& InName = FName::GetNone(), JSON* ActorJsonData = nullptr);

	TArray<AActor*> LevelActors;	// 레벨이 보유하고 있는 모든 Actor를 배열로 저장합니다.
	FOctree* StaticOctree = nullptr;
	TArray<UPrimitiveComponent*> DynamicPrimitives;

	// 지연 삭제를 위한 리스트
	TArray<AActor*> ActorsToDelete;

	uint64 ShowFlags = static_cast<uint64>(EEngineShowFlags::SF_Primitives) |
		static_cast<uint64>(EEngineShowFlags::SF_Billboard) |
		static_cast<uint64>(EEngineShowFlags::SF_Bounds) |
		static_cast<uint64>(EEngineShowFlags::SF_StaticMesh) |
		static_cast<uint64>(EEngineShowFlags::SF_Text) ;
};
