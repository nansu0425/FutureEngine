#include "pch.h"
#include "Component/Public/ScriptComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Manager/Script/Public/ScriptManager.h"
#include "Manager/Path/Public/PathManager.h"
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
	CallLuaFunction("BeginPlay");

	UE_LOG_SUCCESS("ScriptComponent: Hot reload complete for '%s'", ScriptPath.c_str());
}

void UScriptComponent::SetCommonTable()
{
	//if (!GetOwner())
	//	return;


	//


	//UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	//sol::state& lua = ScriptMgr.GetLuaState();

	//AActor* OwnerActor = GetOwner();

	//Table["_actor"] = OwnerActor;
	//Table["Velocity"] = FVector(0.0f, 0.0f, 0.0f);
	//sol::table mt = lua.create_table();

	//// __index: 테이블 또는 Actor에서 프로퍼티 읽기
	//mt[sol::meta_function::index] = [](sol::table self, const std::string& key) -> sol::object {
	//	AActor* actor = self["_actor"];
	//	sol::state_view lua = self.lua_state();

	//	// Actor 프로퍼티 처리
	//	if (key == "UUID")
	//	{
	//		return sol::make_object(lua, actor->GetUUID());
	//	}
	//	else if (key == "Location")
	//	{
	//		return sol::make_object(lua, actor->GetActorLocation());
	//	}
	//	else if (key == "Rotation")
	//	{
	//		return sol::make_object(lua, actor->GetActorRotation());
	//	}
	//	else if (key == "PrintLocation")
	//	{
	//		// 위치를 출력하는 함수 반환
	//		return sol::make_object(lua, [actor]() {
	//			FVector loc = actor->GetActorLocation();
	//			UE_LOG("Location %.2f %.2f %.2f", loc.X, loc.Y, loc.Z);
	//		});
	//	}

	//	// 동적 프로퍼티에 대한 원시 테이블 접근 (Velocity 등)
	//	sol::object val = self.raw_get<sol::object>(key);
	//	return val;
	//};

	//// __newindex: Actor 또는 테이블에 프로퍼티 쓰기
	//mt[sol::meta_function::new_index] = [](sol::table self, const std::string& key, sol::object value) {
	//	AActor* actor = self["_actor"];

	//	// Actor에 다시 쓰여야 하는 프로퍼티 처리
	//	if (key == "Location")
	//	{
	//		FVector newLoc = value.as<FVector>();
	//		actor->SetActorLocation(newLoc);
	//	}
	//	else if (key == "Rotation")
	//	{
	//		FQuaternion newRot = value.as<FQuaternion>();
	//		actor->SetActorRotation(newRot);
	//	}
	//	else
	//	{
	//		// 테이블에 동적 프로퍼티 저장 (Velocity, 커스텀 프로퍼티 등)
	//		self.raw_set(key, value);
	//	}
	//};

	//// 메타테이블 부착
	//Table[sol::metatable_key] = mt;

	//// 테스트: globals 사용 시에는 여기서 obj를 등록하지 않음
	//// (LoadScript에서 lua["obj"] = *ObjTable로 설정함)
}

//void UScriptComponent::CleanupLuaResources()
//{
//	// ScriptManager에서 등록 해제 (BeginPlay()에서 등록했으면 해제 필요)
//	// 처음부터 실패한 스크립트도 등록은 되어있으므로 ScriptPath 기준으로 해제
//	if (!ScriptPath.empty())
//	{
//		UScriptManager::GetInstance().UnregisterScriptComponent(this);
//	}
//
//	if (ObjTable)
//	{
//		delete ObjTable;
//		ObjTable = nullptr;
//	}
//
//	if (ScriptEnv)
//	{
//		delete ScriptEnv;
//		ScriptEnv = nullptr;
//	}
//
//	bScriptLoaded = false;
//}

//void UScriptComponent::ReloadScript()
//{
//	if (ScriptPath.empty())
//	{
//		UE_LOG_WARNING("ScriptComponent: 리로드할 스크립트 경로가 비어있습니다.");
//		return;
//	}
//
//	UE_LOG_INFO("ScriptComponent: 스크립트 리로드 시작 - %s", ScriptPath.c_str());
//
//	// ========== 백업: 기존 상태 저장 (Rollback용) ==========
//	sol::table* OldObjTable = ObjTable;
//	sol::environment* OldScriptEnv = ScriptEnv;
//	bool bWasLoaded = bScriptLoaded;
//
//	// 새 리소스로 초기화 (백업은 유지)
//	ObjTable = nullptr;
//	ScriptEnv = nullptr;
//	bScriptLoaded = false;
//
//	// 1. EndPlay 호출 (백업된 리소스 사용)
//	if (bWasLoaded && OldObjTable)
//	{
//		try
//		{
//			UScriptManager& ScriptMgr = UScriptManager::GetInstance();
//			sol::state& lua = ScriptMgr.GetLuaState();
//			lua["obj"] = *OldObjTable;
//			CallLuaFunction("EndPlay");
//		}
//		catch (const std::exception& e)
//		{
//			UE_LOG_WARNING("ScriptComponent: EndPlay 호출 중 예외 발생 (무시됨): %s", e.what());
//		}
//	}
//
//	// 2. ScriptManager 등록은 BeginPlay()에서 이미 처리되어 유지됨
//	//    ReloadScript()는 Lua 리소스만 교체하고 등록 상태는 건드리지 않음
//
//	// 3. 스크립트 재로드 시도
//	bool bReloadSuccess = LoadScript();
//
//	if (bReloadSuccess)
//	{
//		// ========== 성공: 백업 삭제 ==========
//		if (OldObjTable)
//		{
//			delete OldObjTable;
//			OldObjTable = nullptr;
//		}
//		if (OldScriptEnv)
//		{
//			delete OldScriptEnv;
//			OldScriptEnv = nullptr;
//		}
//
//		// 4. BeginPlay 재호출
//		try
//		{
//			CallLuaFunction("BeginPlay");
//			UE_LOG_SUCCESS("ScriptComponent: 스크립트 Hot Reload 완료 - %s", ScriptPath.c_str());
//		}
//		catch (const std::exception& e)
//		{
//			UE_LOG_ERROR("ScriptComponent: BeginPlay 호출 중 예외 발생: %s", e.what());
//		}
//	}
//	else
//	{
//		// ========== 실패: Rollback (이전 상태로 복원) ==========
//
//		// 새로 생성된 리소스 정리 (실패했으므로)
//		if (ObjTable)
//		{
//			delete ObjTable;
//			ObjTable = nullptr;
//		}
//		if (ScriptEnv)
//		{
//			delete ScriptEnv;
//			ScriptEnv = nullptr;
//		}
//
//		// 백업된 리소스 복원
//		ObjTable = OldObjTable;
//		ScriptEnv = OldScriptEnv;
//		bScriptLoaded = bWasLoaded;
//
//		// Lua globals에 obj 복원
//		if (bWasLoaded && ObjTable)
//		{
//			UScriptManager& ScriptMgr = UScriptManager::GetInstance();
//			sol::state& lua = ScriptMgr.GetLuaState();
//			lua["obj"] = *ObjTable;
//
//			// ScriptManager 재등록 불필요 (BeginPlay()에서 이미 등록됨)
//		}
//
//		// ========== Rollback 에러 로그 (디버깅용) ==========
//		if (bWasLoaded)
//		{
//			UE_LOG_ERROR("ScriptComponent: Hot Reload 실패! 이전 정상 상태로 Rollback 완료 - %s", ScriptPath.c_str());
//			UE_LOG_WARNING("  -> Actor는 이전 스크립트로 계속 동작합니다.");
//		}
//		else
//		{
//			UE_LOG_ERROR("ScriptComponent: 스크립트 리로드 실패 (이전 상태 없음) - %s", ScriptPath.c_str());
//		}
//	}
//}

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
