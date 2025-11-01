#include "pch.h"
#include "Component/Public/ScriptComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Manager/Script/Public/ScriptManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Manager/Script/Public/CoroutineManager.h"
#include "Actor/Public/Actor.h"
#include "json.hpp"

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

UScriptComponent::UScriptComponent()
	: ScriptPath("")
{
	// Tick 활성화
	bCanEverTick = true;
}

UScriptComponent::~UScriptComponent()
{
	UCoroutineManager::GetInstance().StopAllCoroutine(this);
	// CleanupLuaResources();
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ScriptPath.empty())
	{
		return;
	}

	SetInstanceTable(UScriptManager::GetInstance().GetTable(ScriptPath));

	// ScriptManager에 Hot Reload 알림을 받기 위해 등록
	UScriptManager::GetInstance().RegisterScriptComponent(ScriptPath, this);

	// BeginPlay 호출
	CallLuaFunction("BeginPlay");

	// Overlap 델리게이트 바인딩
	BindOverlapDelegates();
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	CallLuaFunction("Tick", DeltaTime);

}

void UScriptComponent::EndPlay()
{
	// EndPlay 호출
	CallLuaFunction("EndPlay");

	// Overlap 델리게이트 해제
	UnbindOverlapDelegates();

	// ScriptManager에서 등록 해제
	if (!ScriptPath.empty())
	{
		UScriptManager::GetInstance().UnregisterScriptComponent(ScriptPath, this);
	}

	Super::EndPlay();
}

void UScriptComponent::TriggerOnOverlap(AActor* OtherActor)
{
	CallLuaFunction("OnOverlap", OtherActor);
}

void UScriptComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		if (InOutHandle.hasKey("ScriptPath"))
		{
			ScriptPath = InOutHandle["ScriptPath"].ToString();
			// NOTE: SetInstanceTable은 BeginPlay에서 호출됨
			// Serialize에서 호출하면 RegisterScriptComponent가 누락되어 Hot reload 알림을 받지 못함
		}
	}
	else
	{
		InOutHandle["ScriptPath"] = ScriptPath;
	}
}

UObject* UScriptComponent::Duplicate()
{
	UScriptComponent* DuplicatedComp = Cast<UScriptComponent>(Super::Duplicate());

	// ScriptPath 복사 (Super::Duplicate()는 NewObject()를 호출하여 생성자에서 초기화되므로 명시적 복사 필요)
	DuplicatedComp->ScriptPath = ScriptPath;

	// PIE 복제 시 Lua 리소스는 초기화하지 않음
	// BeginPlay()에서 각 World별로 독립적인 리소스 생성
	// (InstanceEnv와 CachedFunctions는 BeginPlay에서 SetInstanceTable을 통해 재생성)

	return DuplicatedComp;
}

void UScriptComponent::SetInstanceTable(const sol::table GlobalTable)
{
	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	sol::state& lua = ScriptMgr.GetLuaState();

	// 1. Instance Environment 생성 (GlobalTable을 fallback으로)
	//    Environment chain: InstanceEnv -> GlobalTable(ScriptEnv) -> lua.globals()
	InstanceEnv = sol::environment(lua, sol::create, GlobalTable);

	// 2. Instance 데이터 설정
	AActor* owner = GetOwner();
	if (owner)
	{
		// obj를 Proxy Table로 생성 (Actor 접근 + 동적 프로퍼티 저장 지원)
		sol::table objProxy = lua.create_table();

		// _actor 필드에 실제 Actor 포인터 저장 (내부용)
		objProxy["_actor"] = owner;

		// Metatable 설정
		sol::table mt = lua.create_table();

		// __index: Actor property 또는 동적 프로퍼티 읽기
		// CRITICAL: owner를 캡처하지 않고 매번 테이블에서 가져옴 (댕글링 포인터 방지)
		mt[sol::meta_function::index] = [](sol::table self, const std::string& key) -> sol::object {
			sol::state_view lua = self.lua_state();

			// 동적 프로퍼티 먼저 확인 (raw_get으로 메타메서드 우회)
			sol::object val = self.raw_get<sol::object>(key);
			if (val.valid() && val.get_type() != sol::type::lua_nil)
			{
				return val;
			}

			// Actor 포인터를 매번 테이블에서 가져옴 (댕글링 포인터 방지)
			AActor* owner = self.raw_get<AActor*>("_actor");
			if (!owner)
				return sol::lua_nil;

			// Actor의 기본 프로퍼티 처리
			if (key == "Location")
			{
				return sol::make_object(lua, owner->GetActorLocation());
			}
			else if (key == "Rotation")
			{
				return sol::make_object(lua, owner->GetActorRotation());
			}
			else if (key == "UUID")
			{
				return sol::make_object(lua, owner->GetUUID());
			}
			else if (key == "Name")
			{
				return sol::make_object(lua, owner->GetName().ToString());
			}

			return sol::lua_nil;
		};

		// __newindex: Actor property 또는 동적 프로퍼티 쓰기
		// CRITICAL: owner를 캡처하지 않고 매번 테이블에서 가져옴 (댕글링 포인터 방지)
		mt[sol::meta_function::new_index] = [](sol::table self, const std::string& key, sol::object value) {
			// Actor 포인터를 매번 테이블에서 가져옴 (댕글링 포인터 방지)
			AActor* owner = self.raw_get<AActor*>("_actor");
			if (!owner)
				return;

			// Actor의 기본 프로퍼티 처리
			if (key == "Location")
			{
				FVector newLoc = value.as<FVector>();
				owner->SetActorLocation(newLoc);
			}
			else if (key == "Rotation")
			{
				FQuaternion newRot = value.as<FQuaternion>();
				owner->SetActorRotation(newRot);
			}
			else
			{
				// 동적 프로퍼티를 테이블에 저장 (Velocity, Speed, OverlapCount 등)
				self.raw_set(key, value);
			}
		};

		objProxy[sol::metatable_key] = mt;

		// InstanceEnv에 obj 등록
		InstanceEnv["obj"] = objProxy;

		// UUID는 직접 접근 가능하도록 (선택사항)
		InstanceEnv["UUID"] = owner->GetUUID();

		// Helper 함수 - this를 캡처하여 매번 GetOwner()로 가져옴 (댕글링 포인터 방지)
		InstanceEnv["GetLocation"] = [this]() {
			AActor* owner = GetOwner();
			return owner ? owner->GetActorLocation() : FVector(0, 0, 0);
		};
		InstanceEnv["SetLocation"] = [this](const FVector& v) {
			AActor* owner = GetOwner();
			if (owner) owner->SetActorLocation(v);
		};
		InstanceEnv["PrintLocation"] = [this]() {
			AActor* owner = GetOwner();
			if (owner)
			{
				FVector loc = owner->GetActorLocation();
				UE_LOG("Location: (%.2f, %.2f, %.2f)", loc.X, loc.Y, loc.Z);
			}
		};

		UCoroutineManager::GetInstance().RegisterEnvFunc(this, InstanceEnv);

	}

	// 3. 스크립트 함수들을 캐싱하고 environment 설정
	//    이렇게 하면 함수 호출 시마다 environment를 설정할 필요 없음
	CachedFunctions.clear();

	const char* FunctionNames[] = { "BeginPlay", "Tick", "EndPlay", "OnBeginOverlap", "OnEndOverlap" };
	for (const char* FuncName : FunctionNames)
	{
		sol::optional<sol::function> func_opt = GlobalTable[FuncName];
		if (func_opt)
		{
			sol::function func = *func_opt;
			// 함수의 environment를 InstanceEnv로 설정
			// 이제 함수 내에서 obj, UUID 등에 직접 접근 가능
			InstanceEnv.set_on(func);
			CachedFunctions[FString(FuncName)] = func;
		}
	}

	UE_LOG_DEBUG("ScriptComponent: Instance environment 초기화 완료 (%d functions cached)",
	             static_cast<int32>(CachedFunctions.size()));
}

void UScriptComponent::OnScriptReloaded(const sol::table& NewGlobalTable)
{
	UE_LOG_DEBUG("ScriptComponent: Hot reload notification received for '%s'", ScriptPath.c_str());

	// 새 GlobalTable로 InstanceEnv와 CachedFunctions 재생성
	SetInstanceTable(NewGlobalTable);

	// Hot reload 후 BeginPlay()를 다시 호출하여 동적 프로퍼티 재초기화
	// (obj.Velocity, obj.Speed 등 스크립트에서 설정한 변수들)
	//CallLuaFunction("BeginPlay");

	UE_LOG_SUCCESS("ScriptComponent: Hot reload complete for '%s'", ScriptPath.c_str());
}
/*-----------------------------------------------------------------------------
	Overlap Delegate Binding
-----------------------------------------------------------------------------*/

void UScriptComponent::BindOverlapDelegates()
{
	AActor* Owner = GetOwner();
	/*if (!Owner || !bScriptLoaded)
		return;*/
	if (!Owner)
		return;

	// Owner Actor의 모든 Component 순회
	const TArray<UActorComponent*>& Components = Owner->GetOwnedComponents();

	for (UActorComponent* Component : Components)
	{
		// PrimitiveComponent만 처리
		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (!PrimComp)
			continue;

		// *** CRITICAL: Overlap 이벤트 생성 활성화 ***
		// bGenerateOverlapEvents가 false면 UpdateOverlaps()가 바로 리턴하므로
		// 델리게이트 바인딩만으로는 충분하지 않음 - 반드시 활성화 필요
		PrimComp->SetGenerateOverlapEvents(true);

		// BeginOverlap 델리게이트 바인딩
		FDelegateHandle BeginHandle = PrimComp->OnComponentBeginOverlap.AddWeakLambda(
			this,  // UObject for weak reference
			[this](const FOverlapInfo& OverlapInfo)
			{
				OnBeginOverlapCallback(OverlapInfo);
			}
		);

		// 핸들 저장 (나중에 해제용)
		BeginOverlapHandles.push_back(TPair<UPrimitiveComponent*, FDelegateHandle>(PrimComp, BeginHandle));

		// EndOverlap 델리게이트 바인딩
		FDelegateHandle EndHandle = PrimComp->OnComponentEndOverlap.AddWeakLambda(
			this,  // UObject for weak reference
			[this](const FOverlapInfo& OverlapInfo)
			{
				OnEndOverlapCallback(OverlapInfo);
			}
		);

		// 핸들 저장
		EndOverlapHandles.push_back(TPair<UPrimitiveComponent*, FDelegateHandle>(PrimComp, EndHandle));
	}

	UE_LOG_DEBUG("ScriptComponent: Overlap 델리게이트 바인딩 완료 (%d PrimitiveComponents, GenerateOverlapEvents=true)",
	             static_cast<int32>(BeginOverlapHandles.size()));
}

void UScriptComponent::UnbindOverlapDelegates()
{
	// BeginOverlap 핸들 제거
	for (const auto& Pair : BeginOverlapHandles)
	{
		UPrimitiveComponent* Comp = Pair.first;
		FDelegateHandle Handle = Pair.second;

		if (Comp && Handle.IsValid())
		{
			Comp->OnComponentBeginOverlap.Remove(Handle);
		}
	}
	BeginOverlapHandles.clear();

	// EndOverlap 핸들 제거
	for (const auto& Pair : EndOverlapHandles)
	{
		UPrimitiveComponent* Comp = Pair.first;
		FDelegateHandle Handle = Pair.second;

		if (Comp && Handle.IsValid())
		{
			Comp->OnComponentEndOverlap.Remove(Handle);
		}
	}
	EndOverlapHandles.clear();
}

void UScriptComponent::OnBeginOverlapCallback(const FOverlapInfo& OverlapInfo)
{
	/*if (!bScriptLoaded)
		return;*/

	// 상대 Actor 가져오기
	AActor* OtherActor = OverlapInfo.OverlappingComponent ?
	                     OverlapInfo.OverlappingComponent->GetOwner() : nullptr;

	if (!OtherActor)
		return;

	// Lua 함수 호출: OnBeginOverlap(OtherActor)
	CallLuaFunction("OnBeginOverlap", OtherActor);
}

void UScriptComponent::OnEndOverlapCallback(const FOverlapInfo& OverlapInfo)
{
	/*if (!bScriptLoaded)
		return;*/

	// 상대 Actor 가져오기
	AActor* OtherActor = OverlapInfo.OverlappingComponent ?
	                     OverlapInfo.OverlappingComponent->GetOwner() : nullptr;

	if (!OtherActor)
		return;

	// Lua 함수 호출: OnEndOverlap(OtherActor)
	CallLuaFunction("OnEndOverlap", OtherActor);
}
