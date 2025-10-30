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
		// 디버깅: Vector가 globals에 제대로 등록되었는지 확인
		sol::object vec_in_globals = lua["Vector"];
		UE_LOG("Vector in globals - type: %d, valid: %d", (int)vec_in_globals.get_type(), vec_in_globals.valid());

		// Vector의 타입이 userdata인지 확인
		if (vec_in_globals.is<sol::table>()) {
			sol::table vec_table = vec_in_globals.as<sol::table>();
			UE_LOG("Vector is a table");

			// Metatable 확인
			sol::optional<sol::table> mt = vec_table[sol::metatable_key];
			if (mt) {
				UE_LOG("Vector has metatable");

				// __call 메타메서드 확인
				sol::object call_meta = (*mt)[sol::meta_function::call];
				if (call_meta.valid()) {
					UE_LOG("Vector metatable has __call: type=%d", (int)call_meta.get_type());
				} else {
					UE_LOG_ERROR("Vector metatable has NO __call!");
				}
			} else {
				UE_LOG_ERROR("Vector has NO metatable!");
			}
		}

		// 테스트: Lua에서 직접 Vector 호출 시도
		lua.script(R"(
			print("Testing Vector call from C++...")
			local success, result = pcall(function()
				local v = Vector(1, 2, 3)
				print("Vector created successfully!")
				return v
			end)
			if not success then
				print("Vector call failed: " .. tostring(result))
			end
		)");

		// 환경 없이 직접 globals 사용 (테스트)
		ScriptEnv = nullptr;

		// Owner Actor를 래핑하는 "obj" 테이블 생성
		CreateObjTable();

		// obj를 globals에 직접 설정
		lua["obj"] = *ObjTable;

		// 전체 스크립트 경로 구성
		path FullPath = UPathManager::GetInstance().GetDataPath() / "Scripts" / ScriptPath.c_str();
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
	if (!GetOwner())
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
