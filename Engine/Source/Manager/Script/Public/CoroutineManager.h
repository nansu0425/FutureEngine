#pragma once
#include "Core/Public/Object.h"
#include <filesystem>

// Sol2 헤더 (단일 헤더 라이브러리)
#define SOL_ALL_SAFETIES_ON 1
#include <Sol2/sol.hpp>

class UScriptComponent;

/**
 * UScriptManager
 *
 * Lua 스크립팅을 위한 싱글톤 매니저 (Sol2 사용)
 * 전역 Lua 상태를 관리하고 엔진 타입에 대한 API 바인딩을 제공합니다.
 */

enum class EWaitType
{
	None,
	Time,
	Lambda,
};
struct FWaitCondition
{
	float WaitTime = 0.0f;
	EWaitType WaitType = EWaitType::None;
	std::function<bool()> Lambda;
	//람다
	FWaitCondition() = default;
	FWaitCondition(float InWaitTime)
	{
		WaitTime = InWaitTime;
		WaitType = EWaitType::Time;
	}

	FWaitCondition(std::function<bool()> InLambda)
	{
		Lambda = InLambda;
		WaitType = EWaitType::Lambda;
	}
};
UCLASS()
class UCoroutineManager : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UCoroutineManager, UObject)


	struct CoroutineData
	{
		sol::thread Thread;
		sol::coroutine Coroutine;
		UScriptComponent* Comp;
		FWaitCondition WaitCondition;
		sol::reference GCRef;
		FName FuncName;
	};
	struct PendingCoroutineData
	{
		FName FuncName;
		UScriptComponent* Comp;
	};
private:
	TArray<PendingCoroutineData> PendingCoroutines;
	TArray<CoroutineData> Coroutines;
public:
	// 생성자 및 소멸자
	void Init();
	void RegisterStateFunc();
	void RegisterEnvFunc(UScriptComponent* Comp, sol::environment& Env);
	void RegisterPendingCoroutine(UScriptComponent* Comp, const FString& FuncName);
	void MakeCoroutine(UScriptComponent* Comp, const FString& FuncName);
	void StopCoroutine(UScriptComponent* Comp, const FString& FuncName);
	bool ResumeCoroutine(CoroutineData& Coroutine);
	void Update(const float DeltaTime);
	void StopAllCoroutine(UScriptComponent* Comp);


private:


};
