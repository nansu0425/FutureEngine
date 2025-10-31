#include "pch.h"
#include "Component/Public/ScriptComponent.h"
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
	CleanupLuaResources();
}

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ScriptPath.empty())
	{
		return;
	}

	SetInstanceTable(UScriptManager::GetInstance().GetTable(ScriptPath));

	// ScriptManager에 Hot Reload 등록 (로드 성공 여부와 무관)
	// 처음부터 에러가 있는 스크립트도 수정 시 Hot Reload 되도록
	CallLuaFunction("BeginPlay");
}

void UScriptComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	CallLuaFunction("Tick", DeltaTime);
}

void UScriptComponent::EndPlay()
{
	CallLuaFunction("EndPlay");
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
	DuplicatedComp->Table = Table;

	// ScriptPath 복사 (Super::Duplicate()는 NewObject()를 호출하여 생성자에서 초기화되므로 명시적 복사 필요)
	DuplicatedComp->ScriptPath = ScriptPath;

	return DuplicatedComp;
}

float TestSpeed = 5;
void UScriptComponent::SetInstanceTable(const sol::table GlobalTable)
{
	UScriptManager& ScriptMgr = UScriptManager::GetInstance();
	sol::state& lua = ScriptMgr.GetLuaState();

	// 1. 빈 인스턴스 테이블 생성
	Table = lua.create_table();

	// 2. 메타테이블 설정 (핵심!)
	sol::table metatable = lua.create_table();
	metatable[sol::meta_function::index] = GlobalTable;  // __index를 GlobalTable로
	Table[sol::metatable_key] = metatable;

	// 4. Lua의 new 함수가 있다면 호출
	if (GlobalTable["new"].valid()) {
		sol::function newFunc = GlobalTable["new"];
		Table = newFunc(GlobalTable, TestSpeed);  // self를 전달
		TestSpeed -= 10;
	}

	AActor* owner = GetOwner();
	if (owner) {
		// obj 객체
		Table["obj"] = owner;

		// 속성들
		Table["UUID"] = owner->GetUUID();

		// Location은 함수로 제공
		Table["GetLocation"] = [owner]() { return owner->GetActorLocation(); };
		Table["SetLocation"] = [owner](const FVector& v) { owner->SetActorLocation(v); };

		// Velocity
		Table["Velocity"] = FVector(0.0f, 0.0f, 0.0f);

		// Helper
		Table["PrintLocation"] = [owner]() {
			FVector loc = owner->GetActorLocation();
			UE_LOG("Location: (%.2f, %.2f, %.2f)", loc.X, loc.Y, loc.Z);
			};
	}
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

void UScriptComponent::CleanupLuaResources()
{
}
