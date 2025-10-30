#include "pch.h"
#include "Component/Public/ScriptComponent.h"
#include "Manager/Script/Public/ScriptManager.h"
#include "Manager/Path/Public/PathManager.h"
#include "Actor/Public/Actor.h"
#include "json.hpp"

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

UScriptComponent::UScriptComponent()
	: ScriptPath("")
	, ScriptEnv(nullptr)
	, ObjTable(nullptr)
	, bScriptLoaded(false)
{
}

UScriptComponent::~UScriptComponent()
{
	CleanupLuaResources();
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ScriptPath.empty())
	{
		UE_LOG_WARNING("ScriptComponent on %s has no script path set", GetOwner()->GetName().ToString().c_str());
		return;
	}

	// 스크립트 로드
	if (LoadScript())
	{
		// BeginPlay 호출
		CallLuaFunction("BeginPlay");
	}
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (bScriptLoaded)
	{
		// obj.Location을 Actor와 동기화 (Lua 외부에서 수정된 경우 대비)
		if (ObjTable && GetOwner())
		{
			(*ObjTable)["Location"] = GetOwner()->GetActorLocation();
		}

		// Tick 호출
		CallLuaFunction("Tick", DeltaTime);
	}
}

void UScriptComponent::EndPlay()
{
	if (bScriptLoaded)
	{
		// EndPlay 호출
		CallLuaFunction("EndPlay");
	}

	CleanupLuaResources();

	Super::EndPlay();
}

void UScriptComponent::TriggerOnOverlap(AActor* OtherActor)
{
	if (bScriptLoaded && OtherActor)
	{
		CallLuaFunction("OnOverlap", OtherActor);
	}
}

void UScriptComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		if (InOutHandle.hasKey("ScriptPath"))
		{
			ScriptPath = InOutHandle["ScriptPath"].ToString();
		}
	}
	else
	{
		InOutHandle["ScriptPath"] = ScriptPath;
	}
}

bool UScriptComponent::LoadScript()
{
	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	sol::state& lua = ScriptMgr.GetLuaState();

	try
	{
		// 이 스크립트를 위한 격리된 환경 생성
		ScriptEnv = new sol::environment(lua, sol::create, lua.globals());

		// Owner Actor를 래핑하는 "obj" 테이블 생성
		CreateObjTable();

		// 전체 스크립트 경로 구성
		path FullPath = UPathManager::GetInstance().GetDataPath() / "Scripts" / ScriptPath.c_str();
		FString FullPathStr = FullPath.string();

		// 격리된 환경에서 스크립트 파일 로드 및 실행
		sol::protected_function_result result = lua.script_file(FullPathStr.c_str(), *ScriptEnv);

		if (!result.valid())
		{
			sol::error err = result;
			UE_LOG_ERROR("Lua 스크립트 로드 실패 %s: %s", ScriptPath.c_str(), err.what());
			CleanupLuaResources();
			return false;
		}

		bScriptLoaded = true;
		UE_LOG_SUCCESS("Lua 스크립트 로드 완료: %s", ScriptPath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("Lua 스크립트 로드 중 예외 발생 %s: %s", ScriptPath.c_str(), e.what());
		CleanupLuaResources();
		return false;
	}
}

void UScriptComponent::CreateObjTable()
{
	if (!ScriptEnv || !GetOwner())
		return;

	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	sol::state& lua = ScriptMgr.GetLuaState();

	AActor* OwnerActor = GetOwner();

	// obj 테이블 생성
	ObjTable = new sol::table(lua.create_table());
	sol::table& obj = *ObjTable;

	// 실제 Actor 참조 저장
	obj["_actor"] = OwnerActor;

	// 기본값으로 동적 프로퍼티 초기화
	obj["Velocity"] = FVector(0.0f, 0.0f, 0.0f);

	// 프로퍼티 접근을 위한 메타테이블 생성
	sol::table mt = lua.create_table();

	// __index: 테이블 또는 Actor에서 프로퍼티 읽기
	mt[sol::meta_function::index] = [](sol::table self, const std::string& key) -> sol::object {
		AActor* actor = self["_actor"];
		sol::state_view lua = self.lua_state();

		// Actor 프로퍼티 처리
		if (key == "UUID")
		{
			return sol::make_object(lua, actor->GetUUID());
		}
		else if (key == "Location")
		{
			return sol::make_object(lua, actor->GetActorLocation());
		}
		else if (key == "Rotation")
		{
			return sol::make_object(lua, actor->GetActorRotation());
		}
		else if (key == "PrintLocation")
		{
			// 위치를 출력하는 함수 반환
			return sol::make_object(lua, [actor]() {
				FVector loc = actor->GetActorLocation();
				UE_LOG("Location %.2f %.2f %.2f", loc.X, loc.Y, loc.Z);
			});
		}

		// 동적 프로퍼티에 대한 원시 테이블 접근 (Velocity 등)
		sol::object val = self.raw_get<sol::object>(key);
		return val;
	};

	// __newindex: Actor 또는 테이블에 프로퍼티 쓰기
	mt[sol::meta_function::new_index] = [](sol::table self, const std::string& key, sol::object value) {
		AActor* actor = self["_actor"];

		// Actor에 다시 쓰여야 하는 프로퍼티 처리
		if (key == "Location")
		{
			FVector newLoc = value.as<FVector>();
			actor->SetActorLocation(newLoc);
		}
		else if (key == "Rotation")
		{
			FQuaternion newRot = value.as<FQuaternion>();
			actor->SetActorRotation(newRot);
		}
		else
		{
			// 테이블에 동적 프로퍼티 저장 (Velocity, 커스텀 프로퍼티 등)
			self.raw_set(key, value);
		}
	};

	// 메타테이블 부착
	obj[sol::metatable_key] = mt;

	// 스크립트 환경에 obj 등록
	(*ScriptEnv)["obj"] = obj;
}

void UScriptComponent::CleanupLuaResources()
{
	if (ObjTable)
	{
		delete ObjTable;
		ObjTable = nullptr;
	}

	if (ScriptEnv)
	{
		delete ScriptEnv;
		ScriptEnv = nullptr;
	}

	bScriptLoaded = false;
}
