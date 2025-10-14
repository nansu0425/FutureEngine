#include "pch.h"
#include "Level/Public/Level.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"
#include "Core/Public/Object.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/Viewport.h"
#include "Utility/Public/JsonSerializer.h"
#include "Global/Octree.h"
#include <json.hpp>

IMPLEMENT_CLASS(ULevel, UObject)

ULevel::ULevel()
{
	StaticOctree = new FOctree(FVector(0, 0, -5), 75, 0);
}

ULevel::~ULevel()
{
	// LevelActors 배열에 남아있는 모든 액터의 메모리를 해제합니다.
	for (const auto& Actor : LevelActors)
	{
		DestroyActor(Actor);
	}
	LevelActors.clear();

	// 모든 액터 객체가 삭제되었으므로, 포인터를 담고 있던 컨테이너들을 비웁니다.
	SafeDelete(StaticOctree);
}

void ULevel::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		// NOTE: 레벨 로드 시 NextUUID를 변경하면 UUID 충돌이 발생하므로 관련 기능 구현을 보류합니다.
		uint32 NextUUID = 0;
		FJsonSerializer::ReadUint32(InOutHandle, "NextUUID", NextUUID);

		JSON PerspectiveCameraData;
		if (FJsonSerializer::ReadArray(InOutHandle, "PerspectiveCamera", PerspectiveCameraData))
		{
			UConfigManager::GetInstance().SetCameraSettingsFromJson(PerspectiveCameraData);
			URenderer::GetInstance().GetViewportClient()->ApplyAllCameraDataToViewportClients();
		}
		
		JSON ActorsJson;
		if (FJsonSerializer::ReadObject(InOutHandle, "Actors", ActorsJson))
		{
			for (auto& Pair : ActorsJson.ObjectRange())
			{
				const FString& IdString = Pair.first;
				JSON& ActorDataJson = Pair.second;

				FString TypeString;
				FJsonSerializer::ReadString(ActorDataJson, "Type", TypeString);
				
				UClass* ActorClass = UClass::FindClass(TypeString);
				AActor* NewActor = SpawnActorToLevel(ActorClass, IdString, &ActorDataJson); 
			}
		}
	}

	// 저장
	else
	{
		// NOTE: 레벨 로드 시 NextUUID를 변경하면 UUID 충돌이 발생하므로 관련 기능 구현을 보류합니다.
		InOutHandle["NextUUID"] = 0;

		// GetCameraSetting 호출 전에 뷰포트 클라이언트의 최신 데이터를 ConfigManager로 동기화합니다.
		URenderer::GetInstance().GetViewportClient()->UpdateCameraSettingsToConfig();
		InOutHandle["PerspectiveCamera"] = UConfigManager::GetInstance().GetCameraSettingsAsJson();

		JSON ActorsJson = json::Object();
		for (AActor* Actor : LevelActors)
		{
			JSON ActorJson;
			ActorJson["Type"] = Actor->GetClass()->GetName().ToString();
			Actor->Serialize(bInIsLoading, ActorJson); 

			ActorsJson[std::to_string(Actor->GetUUID())] = ActorJson;
		}
		InOutHandle["Actors"] = ActorsJson;
	}
}

void ULevel::Init()
{
	// TEST CODE
}

AActor* ULevel::SpawnActorToLevel(UClass* InActorClass, const FName& InName, JSON* ActorJsonData)
{
	if (!InActorClass)
	{
		return nullptr;
	}

	AActor* NewActor = Cast<AActor>(NewObject(InActorClass));
	if (NewActor)
	{
		if (!InName.IsNone())
		{
			NewActor->SetName(InName);
		}
		LevelActors.push_back(NewActor);
		if (ActorJsonData != nullptr)
		{
			NewActor->Serialize(true, *ActorJsonData);
		}
		else
		{
			NewActor->InitializeComponents();
		}
		NewActor->BeginPlay();
		AddLevelPrimitiveComponent(NewActor);
		return NewActor;
	}

	return nullptr;
}

void ULevel::RegisterPrimitiveComponent(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (!StaticOctree)
	{
		return;
	}

	// StaticOctree에 먼저 삽입 시도
	if (!(StaticOctree->Insert(InComponent)))
	{
		// 실패하면 DynamicPrimitiveQueue 목록에 추가
		OnPrimitiveUpdated(InComponent);
	}

	UE_LOG("Level: '%s' 컴포넌트를 씬에 등록했습니다.", InComponent->GetName().ToString().data());
}

void ULevel::UnregisterPrimitiveComponent(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (!StaticOctree)
	{
		return;
	}
	
	// StaticOctree에서 제거 시도
	StaticOctree->Remove(InComponent);
	
	OnPrimitiveUnregistered(InComponent);
}

void ULevel::AddLevelPrimitiveComponent(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (auto& Component : Actor->GetOwnedComponents())
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		OnPrimitiveUpdated(PrimitiveComponent);
	}
}

// Level에서 Actor 제거하는 함수
bool ULevel::DestroyActor(AActor* InActor)
{
	if (!InActor)
	{
		return false;
	}

	// 컴포넌트들을 옥트리에서 제거
	for (auto& Component : InActor->GetOwnedComponents())
	{
		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		
		UnregisterPrimitiveComponent(PrimitiveComponent);
	}

	// LevelActors 리스트에서 제거
	if (auto It = std::find(LevelActors.begin(), LevelActors.end(), InActor); It != LevelActors.end())
	{
		*It = std::move(LevelActors.back());
		LevelActors.pop_back();
	}

	// Remove Actor Selection
	UEditor* Editor = GEditor->GetEditorModule();
	if (Editor->GetSelectedActor() == InActor)
	{
		Editor->SelectActor(nullptr);
		Editor->SelectComponent(nullptr);
	}

	// Remove
	SafeDelete(InActor);

	UE_LOG("Level: Actor Destroyed Successfully");
	return true;
}

void ULevel::UpdatePrimitiveInOctree(UPrimitiveComponent* InComponent)
{
	if (!StaticOctree->Remove(InComponent))
		return;
	OnPrimitiveUpdated(InComponent);
}

UObject* ULevel::Duplicate()
{
	ULevel* Level = Cast<ULevel>(Super::Duplicate());
	Level->ShowFlags = ShowFlags;
	return Level;
}

void ULevel::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
	ULevel* DuplicatedLevel = Cast<ULevel>(DuplicatedObject);

	for (AActor* Actor : LevelActors)
	{
		AActor* DuplicatedActor = Cast<AActor>(Actor->Duplicate());
		DuplicatedLevel->LevelActors.push_back(DuplicatedActor);
		DuplicatedLevel->AddLevelPrimitiveComponent(DuplicatedActor);
	}
}

/*-----------------------------------------------------------------------------
	Octree Management
-----------------------------------------------------------------------------*/

void ULevel::UpdateOctree()
{
	if (!StaticOctree)
	{
		return;
	}
	
	uint32 Count = 0;

	while (!DynamicPrimitiveQueue.empty() && Count < MAX_OBJECTS_TO_INSERT_PER_FRAME)
	{
		auto [Component, TimePoint] = DynamicPrimitiveQueue.front();
		DynamicPrimitiveQueue.pop();

		if (auto It = DynamicPrimitiveMap.find(Component); It != DynamicPrimitiveMap.end())
		{
			if (It->second <= TimePoint)
			{
				// 큐에 기록된 오브젝트의 마지막 변경 시간 이후로 변경이 없었다면 Octree에 재삽입한다.
				// TODO: 오브젝트의 유일성을 보장하기 위해 StaticOctree->Remove(Component)가 필요한가?
				StaticOctree->Insert(Component);
				DynamicPrimitiveMap.erase(It);
				++Count;
			}
			else
			{
				// 큐에 기록된 오브젝트의 마지막 변경 이후 새로운 변경이 존재했다면 다시 큐에 삽입한다.
				DynamicPrimitiveQueue.push({Component, It->second});
			}
		}
	}

	if (Count != 0)
	{
		//UE_LOG("UpdateOctree: %d개의 컴포넌트가 업데이트 되었습니다.", Count);
	}
}

void ULevel::OnPrimitiveUpdated(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	float GameTime = UTimeManager::GetInstance().GetGameTime();
	if (auto It = DynamicPrimitiveMap.find(InComponent); It != DynamicPrimitiveMap.end())
	{
		It->second = GameTime;
	}
	else
	{
		DynamicPrimitiveMap[InComponent] = UTimeManager::GetInstance().GetGameTime();

		DynamicPrimitiveQueue.push({InComponent, GameTime});
	}
}

void ULevel::OnPrimitiveUnregistered(UPrimitiveComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	if (auto It = DynamicPrimitiveMap.find(InComponent); It != DynamicPrimitiveMap.end())
	{
		DynamicPrimitiveMap.erase(It);
	}
}
