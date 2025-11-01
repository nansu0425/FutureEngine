#pragma once
#include "Core/Public/Object.h"
#include <filesystem>

// Sol2 헤더 (단일 헤더 라이브러리)
#define SOL_ALL_SAFETIES_ON 1
#include <Sol2/sol.hpp>

// 전방 선언
class UScriptComponent;

struct FLuaScript
{
	FString Path;
	std::filesystem::file_time_type LastCompileTime;
	sol::table GlobalTable;
};

/**
 * UScriptManager
 *
 * Lua 스크립팅을 위한 싱글톤 매니저 (Sol2 사용)
 * 전역 Lua 상태를 관리하고 엔진 타입에 대한 API 바인딩을 제공합니다.
 */
UCLASS()
class UScriptManager : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UScriptManager, UObject)

private:
	sol::state* LuaState;

	// Hot Reload 시스템
	/** 스크립트 경로 → 마지막 수정 시간 */
	TMap<FString, FLuaScript> LuaScriptMap;

	/** Hot Reload 체크 주기 (초 단위) */
	static constexpr float HotReloadCheckInterval = 0.5f;

	/** 마지막 Hot Reload 체크 시간 */
	float LastHotReloadCheckTime = 0.0f;

public:
	/**
	 * Lua VM 초기화 및 엔진 타입 등록
	 * 엔진 시작 시 한 번 호출됩니다.
	 */
	void Initialize();

	/**
	 * Lua VM 종료 및 리소스 정리
	 */
	void Shutdown();

	/**
	 * 전역 Lua 상태 가져오기 (고급 사용자용)
	 * @return Sol2 Lua state 참조
	 */
	sol::state& GetLuaState();

	/**
	 * Lua 스크립트 파일 실행
	 * @param FilePath - .lua 파일의 절대 경로 또는 상대 경로
	 * @return 실행 성공 여부
	 */
	bool ExecuteFile(const FString& FilePath);

	/**
	 * Lua 코드 문자열 실행
	 * @param Code - 실행할 Lua 코드
	 * @return 실행 성공 여부
	 */
	bool ExecuteString(const FString& Code);


	bool LoadLuaScript(const FString& ScriptPath);
	sol::table GetTable(const FString& ScriptPath);
	bool IsLoadedScript(const FString& ScriptPath);

	/**
	 * 변경된 스크립트 파일 경로 수집
	 * @return 변경된 스크립트 파일 경로 집합
	 */
	TSet<FString> GatherHotReloadTargets();

	/**
	 * 변경된 스크립트 파일들을 리로드
	 */
	void HotReloadScripts();

private:
	/**
	 * 엔진 핵심 타입을 Lua에 등록
	 * FVector, AActor, USceneComponent 등을 바인딩합니다.
	 */
	void RegisterCoreTypes();

	/**
	 * 전역 유틸리티 함수를 Lua에 등록
	 * print, ULog 등을 바인딩합니다.
	 */
	void RegisterGlobalFunctions();
};
