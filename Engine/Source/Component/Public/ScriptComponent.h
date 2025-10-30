#pragma once
#include "Component/Public/ActorComponent.h"

// JSON 타입 정의
namespace json { class JSON; }
using JSON = json::JSON;

// Sol2 헤더 (템플릿 함수에서 사용하므로 전체 정의 필요)
#define SOL_ALL_SAFETIES_ON 1
#include <Sol2/sol.hpp>

// ScriptManager 헤더 (템플릿 함수에서 사용)
#include "Manager/Script/Public/ScriptManager.h"

/**
 * UScriptComponent
 *
 * Actor에 Lua 스크립팅 기능을 추가하는 컴포넌트
 * 라이프사이클 콜백 제공: BeginPlay, Tick, EndPlay, OnOverlap
 *
 * Lua 스크립트 내에서 Owner Actor는 전역 "obj" 변수로 접근 가능합니다.
 *
 * 스크립트 예시:
 * ```lua
 * function BeginPlay()
 *     print("Actor started: " .. obj.UUID)
 *     obj.Velocity = Vector(10, 0, 0)
 * end
 *
 * function Tick(dt)
 *     obj.Location = obj.Location + obj.Velocity * dt
 * end
 * ```
 */
UCLASS()
class UScriptComponent : public UActorComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UScriptComponent, UActorComponent)

private:
	/** Lua 스크립트 파일 경로 (Data/Scripts/ 기준 상대 경로) */
	FString ScriptPath;

	/** 이 스크립트를 위한 격리된 Lua 환경 */
	sol::environment* ScriptEnv;

	/** Owner Actor를 래핑하는 Lua 테이블 ("obj" 변수) */
	sol::table* ObjTable;

	/** 스크립트가 성공적으로 로드되었는지 여부 */
	bool bScriptLoaded;

public:
	UScriptComponent();
	virtual ~UScriptComponent() override;

	/**
	 * Lua 스크립트 경로 설정
	 * @param InPath - Data/Scripts/ 기준 상대 경로 (예: "TestScript.lua")
	 */
	void SetScriptPath(const FString& InPath) { ScriptPath = InPath; }

	/**
	 * 현재 스크립트 경로 가져오기
	 */
	const FString& GetScriptPath() const { return ScriptPath; }

	/**
	 * 스크립트가 로드되고 준비되었는지 확인
	 */
	bool IsScriptLoaded() const { return bScriptLoaded; }

	// 라이프사이클 오버라이드
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void EndPlay() override;

	/**
	 * 인자와 함께 Lua 함수 호출
	 * @param FunctionName - 호출할 Lua 함수 이름
	 * @param Args - 함수에 전달할 인자들
	 */
	template<typename... Args>
	void CallLuaFunction(const char* FunctionName, Args&&... args);

	/**
	 * Lua 스크립트의 OnOverlap 이벤트 발생
	 * @param OtherActor - 충돌한 다른 Actor
	 */
	void TriggerOnOverlap(AActor* OtherActor);

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// PIE 복제 시 Lua 리소스 초기화
	virtual UObject* Duplicate() override;

private:
	/**
	 * Lua 스크립트 로드 및 격리된 환경 생성
	 * @return 스크립트 로드 성공 여부
	 */
	bool LoadScript();

	/**
	 * Owner Actor를 래핑하는 "obj" 테이블 생성
	 * 이 테이블은 정적 Actor 프로퍼티(UUID, Location 등)와
	 * 동적 스크립트 프로퍼티(Velocity 등)를 모두 지원합니다.
	 */
	void CreateObjTable();

	/**
	 * Lua 리소스 정리
	 */
	void CleanupLuaResources();
};

// 템플릿 구현
template<typename... Args>
void UScriptComponent::CallLuaFunction(const char* FunctionName, Args&&... args)
{
	if (!bScriptLoaded)
		return;

	try
	{
		UScriptManager& ScriptMgr = UScriptManager::GetInstance();
		sol::state& lua = ScriptMgr.GetLuaState();

		// CRITICAL: globals의 obj를 현재 컴포넌트의 ObjTable로 동기화
		// 여러 World가 같은 Lua state를 공유하므로, 함수 호출 전에 반드시 obj를 설정해야 함
		if (ObjTable)
		{
			lua["obj"] = *ObjTable;
		}

		sol::optional<sol::function> func = lua[FunctionName];
		if (func)
		{
			sol::protected_function_result result = (*func)(std::forward<Args>(args)...);

			if (!result.valid())
			{
				sol::error err = result;
				UE_LOG_ERROR("Lua function '%s' error: %s", FunctionName, err.what());
			}
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("Exception calling Lua function '%s': %s", FunctionName, e.what());
	}
}
