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
	// Tick 활성화
	bCanEverTick = true;
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

UObject* UScriptComponent::Duplicate()
{
	UScriptComponent* DuplicatedComp = Cast<UScriptComponent>(Super::Duplicate());

	// PIE 복제 시 Lua 리소스는 nullptr로 초기화
	// BeginPlay()에서 각 World별로 독립적인 리소스 생성
	DuplicatedComp->ObjTable = nullptr;
	DuplicatedComp->ScriptEnv = nullptr;
	DuplicatedComp->bScriptLoaded = false;

	// ScriptPath 복사 (Super::Duplicate()는 NewObject()를 호출하여 생성자에서 초기화되므로 명시적 복사 필요)
	DuplicatedComp->ScriptPath = ScriptPath;

	return DuplicatedComp;
}

bool UScriptComponent::LoadScript()
{
	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	sol::state& lua = ScriptMgr.GetLuaState();

	try
	{
		// Owner Actor를 래핑하는 "obj" 테이블 생성
		CreateObjTable();

		// obj를 globals에 직접 설정
		lua["obj"] = *ObjTable;

		// 전체 스크립트 경로 구성 (Engine 우선, 없으면 Build)
		UPathManager& PathMgr = UPathManager::GetInstance();
		path EngineScriptPath = PathMgr.GetEngineDataPath() / "Scripts" / ScriptPath.c_str();
		path BuildScriptPath = PathMgr.GetDataPath() / "Scripts" / ScriptPath.c_str();

		path FullPath;
		if (std::filesystem::exists(EngineScriptPath))
		{
			FullPath = EngineScriptPath;
			UE_LOG_INFO("Lua 스크립트 로드 (Engine): %s", FullPath.string().c_str());
		}
		else if (std::filesystem::exists(BuildScriptPath))
		{
			FullPath = BuildScriptPath;
			UE_LOG_INFO("Lua 스크립트 로드 (Build): %s", FullPath.string().c_str());
		}
		else
		{
			UE_LOG_ERROR("Lua 스크립트 파일을 찾을 수 없습니다: %s (Engine: %s, Build: %s)",
				ScriptPath.c_str(), EngineScriptPath.string().c_str(), BuildScriptPath.string().c_str());
			CleanupLuaResources();
			return false;
		}

		FString FullPathStr = FullPath.string();

		// 테스트: 환경 없이 직접 실행 (globals 사용)
		sol::protected_function_result result = lua.script_file(FullPathStr.c_str());

		if (!result.valid())
		{
			sol::error err = result;
			UE_LOG_ERROR("Lua 스크립트 로드 실패 %s: %s", ScriptPath.c_str(), err.what());
			CleanupLuaResources();
			return false;
		}

		bScriptLoaded = true;
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
	if (!GetOwner())
		return;

	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	sol::state& lua = ScriptMgr.GetLuaState();

	AActor* OwnerActor = GetOwner();

	// 기존 ObjTable이 있으면 먼저 정리 (PIE 복제로 인한 메모리 누수 및 dangling pointer 방지)
	if (ObjTable)
	{
		delete ObjTable;
		ObjTable = nullptr;
	}

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

	// 테스트: globals 사용 시에는 여기서 obj를 등록하지 않음
	// (LoadScript에서 lua["obj"] = *ObjTable로 설정함)
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
