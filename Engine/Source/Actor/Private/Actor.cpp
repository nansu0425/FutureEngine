#include "pch.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/SceneComponent.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Level/Public/Level.h"
#include "Utility/Public/ActorTypeMapper.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(AActor, UObject)

AActor::AActor()
{
}

AActor::AActor(UObject* InOuter)
{
	SetOuter(InOuter);
}

AActor::~AActor()
{
	for (UActorComponent* Component : OwnedComponents)
	{
		SafeDelete(Component);
	}
	SetOuter(nullptr);
	OwnedComponents.clear();
}

void AActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    // UObject::Serialize 호출 (상위 UObject 속성 처리)
    // Super::Serialize(bInIsLoading, InOutHandle); 

    // 불러오기 (Load)
    if (bInIsLoading)
    {
    	// 컴포넌트 포인터와 JSON 데이터를 임시 저장할 구조체
        struct FComponentLoadData
        {
            USceneComponent* Component = nullptr;
            FString ParentName; // ParentName을 임시 저장
            JSON ComponentData;     // 모든 JSON 데이터 임시 저장
        };
        TMap<FString, FComponentLoadData> ComponentMap;
        TArray<FComponentLoadData*> LoadList;
        // ----------------------------------------------------
        
        // Components 목록 로드 및 역직렬화 시작
        JSON ComponentsJson;
        if (FJsonSerializer::ReadArray(InOutHandle, "Components", ComponentsJson))
        {
            // --- [PASS 1: Component Creation & Data Load] ---
            for (JSON& ComponentData : ComponentsJson.ArrayRange())
            {
                FString TypeString;
                FString NameString;
                std::string ParentNameStd;
        
                FJsonSerializer::ReadString(ComponentData, "Type", TypeString);
                FJsonSerializer::ReadString(ComponentData, "Name", NameString);
                FJsonSerializer::ReadString(ComponentData, "ParentName", ParentNameStd, ""); // 부모 이름 로드
        
            	UClass* ComponentClass = UClass::FindClass(TypeString);
                USceneComponent* NewComp = Cast<USceneComponent>(NewObject(ComponentClass));
                
                if (NewComp)
                {
                	NewComp->SetOwner(this);
                	OwnedComponents.push_back(NewComp);
                    NewComp->Serialize(bInIsLoading, ComponentData); 
                    
                    FComponentLoadData LoadData;
                    LoadData.Component = NewComp;
                    LoadData.ParentName = ParentNameStd;
                    LoadData.ComponentData = ComponentData;
                    
                    ComponentMap[NameString] = LoadData;
                    LoadList.push_back(&ComponentMap[NameString]);
                }
            }
            
            // --- [PASS 2: Hierarchy Rebuild] ---
            for (FComponentLoadData* LoadDataPtr : LoadList)
            {
                USceneComponent* ChildComp = LoadDataPtr->Component;
                const FString& ParentName = LoadDataPtr->ParentName;
                
                if (!ParentName.empty())
                {
                    // 5. ParentName을 키로 부모 컴포넌트 포인터 검색
                    auto ParentIt = ComponentMap.find(ParentName);
                    if (ParentIt != ComponentMap.end())
                    {
                        USceneComponent* ParentComp = ParentIt->second.Component;
                        // 6. 부착 함수 호출
                        if (ParentComp)
                        {
                            ChildComp->SetParentAttachment(ParentComp);
                        }
                    }
                    else
                    {
                        UE_LOG("Failed to find parent component: %s", ParentName.c_str());
                    }
                }
                // ParentName이 비어있으면 루트 컴포넌트
                else
                {
                	SetRootComponent(ChildComp);
                }
            }

        	if (RootComponent)
        	{
    			FVector Location, Rotation, Scale;
		        
    			FJsonSerializer::ReadVector(InOutHandle, "Location", Location, GetActorLocation());
    			FJsonSerializer::ReadVector(InOutHandle, "Rotation", Rotation, GetActorRotation());
    			FJsonSerializer::ReadVector(InOutHandle, "Scale", Scale, GetActorScale3D());
		        
    			SetActorLocation(Location);
    			SetActorRotation(Rotation);
	    		SetActorScale3D(Scale); 
        	}
        }
    }
    // 저장 (Save)
    else
    {
		InOutHandle["Location"] = FJsonSerializer::VectorToJson(GetActorLocation());
        InOutHandle["Rotation"] = FJsonSerializer::VectorToJson(GetActorRotation());
        InOutHandle["Scale"] = FJsonSerializer::VectorToJson(GetActorScale3D());

        JSON ComponentsJson = json::Array(); 

        for (UActorComponent* Component : OwnedComponents) 
        {
        	USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
        	if (SceneComponent == nullptr) { continue; }
        	
            JSON ComponentJson;
            ComponentJson["Type"] = SceneComponent->GetClass()->GetName().ToString();
            
            FString ComponentName = SceneComponent->GetName().ToString();
        	ComponentJson["Name"] = ComponentName;

            USceneComponent* Parent = SceneComponent->GetParentComponent();
            FString ParentName = Parent ? Parent->GetName().ToString() : "";
        	ComponentJson["ParentName"] = ParentName;

            SceneComponent->Serialize(bInIsLoading, ComponentJson); 
            
            ComponentsJson.append(ComponentJson);
        }
        InOutHandle["Components"] = ComponentsJson;
    }
}


void AActor::SetActorLocation(const FVector& InLocation) const
{
	if (RootComponent)
	{
		RootComponent->SetRelativeLocation(InLocation);
	}
}

void AActor::SetActorRotation(const FVector& InRotation) const
{
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(InRotation);
	}
}

void AActor::SetActorScale3D(const FVector& InScale) const
{
	if (RootComponent)
	{
		RootComponent->SetRelativeScale3D(InScale);
	}
}

void AActor::SetUniformScale(bool IsUniform)
{
	if (RootComponent)
	{
		RootComponent->SetUniformScale(IsUniform);
	}
}

UClass* AActor::GetDefaultRootComponent()
{
	return USceneComponent::StaticClass();
}

void AActor::InitializeComponents()
{
	USceneComponent* SceneComp = Cast<USceneComponent>(CreateDefaultSubobject(GetDefaultRootComponent()));
	SetRootComponent(SceneComp);

	UUIDTextComponent = CreateDefaultSubobject<UUUIDTextComponent>();
	UUIDTextComponent->SetParentAttachment(GetRootComponent());
	UUIDTextComponent->SetOffset(5.0f);
}

bool AActor::IsUniformScale() const
{
	if (RootComponent)
	{
		return RootComponent->IsUniformScale();
	}
	return false;
}

const FVector& AActor::GetActorLocation() const
{
	assert(RootComponent);
	return RootComponent->GetRelativeLocation();
}

const FVector& AActor::GetActorRotation() const
{
	assert(RootComponent);
	return RootComponent->GetRelativeRotation();
}

const FVector& AActor::GetActorScale3D() const
{
	assert(RootComponent);
	return RootComponent->GetRelativeScale3D();
}

UObject* AActor::Duplicate()
{
	AActor* Actor = Cast<AActor>(Super::Duplicate());
	Actor->bCanEverTick = bCanEverTick;
	return Actor;
}

void AActor::RegisterComponent(UActorComponent* InNewComponent)
{
	if (!InNewComponent || InNewComponent->GetOwner() != this)
	{
		InNewComponent->SetOwner(this);
	}

	OwnedComponents.push_back(InNewComponent);

	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(InNewComponent))
	{
		GWorld->GetLevel()->RegisterPrimitiveComponent(PrimitiveComponent);
	}
}

bool AActor::RemoveComponent(UActorComponent* InComponentToDelete)
{
    auto It = std::find(OwnedComponents.begin(), OwnedComponents.end(), InComponentToDelete);
    if (It != OwnedComponents.end())
    {
        if (InComponentToDelete == RootComponent)
        {
			UE_LOG_WARNING("루트 컴포넌트는 제거할 수 없습니다.");
			return false;
        }
        else if (Cast<UUUIDTextComponent>(InComponentToDelete))
        {
			UE_LOG_WARNING("UUIDTextComponent는 제거할 수 없습니다.");
			return false;
        }

        if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(InComponentToDelete))
        {
            GWorld->GetLevel()->UnregisterPrimitiveComponent(PrimitiveComponent);
        }
        if (USceneComponent* SceneComponent = Cast<USceneComponent>(InComponentToDelete))
        {
            if (SceneComponent->GetParentComponent())
            {
                SceneComponent->GetParentComponent()->RemoveChild(SceneComponent);
            }
            for (USceneComponent* Child : SceneComponent->GetChildren())
            {
				RemoveComponent(Child);
            }
		}
        OwnedComponents.erase(It);
        SafeDelete(*It);
        return true;
    }
    return false;
}

void AActor::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
	AActor* DuplicatedActor = Cast<AActor>(DuplicatedObject);
	USceneComponent* DuplicatedRoot = Cast<USceneComponent>(GetRootComponent()->Duplicate());

	TQueue<USceneComponent*> DuplicatedChildren;
	DuplicatedChildren.push(DuplicatedRoot);

	while (DuplicatedChildren.size() > 0)
	{
		USceneComponent* Child = DuplicatedChildren.front();
		DuplicatedChildren.pop();
		
		Child->SetOwner(DuplicatedActor);
		DuplicatedActor->OwnedComponents.push_back(Child);
		for (auto NewChild: Child->GetChildren())
		{
			DuplicatedChildren.push(NewChild);
		}
	}
	
	DuplicatedActor->SetRootComponent(DuplicatedRoot);
	
	for (UActorComponent* Component : OwnedComponents)
	{
		if (!Cast<USceneComponent>(Component))
		{
			UActorComponent* DuplicatedActorComponent = Cast<UActorComponent>(Component->Duplicate());
			DuplicatedActor->OwnedComponents.push_back(DuplicatedActorComponent);
			DuplicatedActorComponent->SetOwner(DuplicatedActor);
		}
	}
}

void AActor::Tick(float DeltaTimes)
{
	for (auto& Component : OwnedComponents)
	{
		if (Component && Component->CanTick())
		{
			Component->TickComponent();
		}
	}
}

void AActor::BeginPlay()
{
	if (bBegunPlay) return;
	bBegunPlay = true;
	for (auto& Component : OwnedComponents)
	{
		if (Component)
		{
			Component->BeginPlay();
		}
	}
}

void AActor::EndPlay()
{
	if (!bBegunPlay) return;
	bBegunPlay = false;
	for (auto& Component : OwnedComponents)
	{
		if (Component)
		{
			Component->EndPlay();
		}
	}
}
