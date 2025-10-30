#include "pch.h"
#include "Manager/Script/Public/ScriptManager.h"

// Sol2 헤더 (단일 헤더 라이브러리)
#define SOL_ALL_SAFETIES_ON 1
#include <Sol2/sol.hpp>

// 엔진 인클루드
#include "Global/Vector.h"
#include "Global/Quaternion.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/SceneComponent.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Render/UI/Window/Public/ConsoleWindow.h"

IMPLEMENT_SINGLETON_CLASS(UScriptManager, UObject)

UScriptManager::UScriptManager()
	: LuaState(nullptr)
{
}

UScriptManager::~UScriptManager()
{
	Shutdown();
}

void UScriptManager::Initialize()
{
	UE_LOG_SYSTEM("Lua 매니저 초기화 중 (Sol2 사용)...");

	try
	{
		// Lua 상태 생성
		LuaState = new sol::state();

		// 표준 Lua 라이브러리 로드
		LuaState->open_libraries(
			sol::lib::base,
			sol::lib::package,
			sol::lib::coroutine,
			sol::lib::string,
			sol::lib::math,
			sol::lib::table,
			sol::lib::debug,
			sol::lib::io
		);

		// 엔진 타입 및 함수 등록
		RegisterCoreTypes();
		RegisterGlobalFunctions();

		UE_LOG_SUCCESS("Lua 매니저 초기화 완료");
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("Lua 매니저 초기화 실패: %s", e.what());
	}
}

void UScriptManager::Shutdown()
{
	if (LuaState)
	{
		delete LuaState;
		LuaState = nullptr;
		UE_LOG_SYSTEM("Lua 매니저 종료");
	}
}

sol::state& UScriptManager::GetLuaState()
{
	if (!LuaState)
	{
		UE_LOG_ERROR("Lua 상태가 null입니다! Initialize()를 호출했나요?");
		static sol::state dummy;
		return dummy;
	}
	return *LuaState;
}

bool UScriptManager::ExecuteFile(const FString& FilePath)
{
	if (!LuaState)
	{
		UE_LOG_ERROR("파일 실행 불가: Lua 상태가 null입니다");
		return false;
	}

	try
	{
		sol::protected_function_result result = LuaState->script_file(FilePath.c_str());

		if (!result.valid())
		{
			sol::error err = result;
			UE_LOG_ERROR("%s 스크립트 오류: %s", FilePath.c_str(), err.what());
			return false;
		}

		UE_LOG_INFO("Lua 파일 실행 완료: %s", FilePath.c_str());
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("Lua 파일 실행 중 예외 발생 %s: %s", FilePath.c_str(), e.what());
		return false;
	}
}

bool UScriptManager::ExecuteString(const FString& Code)
{
	if (!LuaState)
	{
		UE_LOG_ERROR("문자열 실행 불가: Lua 상태가 null입니다");
		return false;
	}

	try
	{
		sol::protected_function_result result = LuaState->script(Code.c_str());

		if (!result.valid())
		{
			sol::error err = result;
			UE_LOG_ERROR("Lua 코드 오류: %s", err.what());
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG_ERROR("Lua 코드 실행 중 예외 발생: %s", e.what());
		return false;
	}
}

void UScriptManager::RegisterCoreTypes()
{
	sol::state& lua = *LuaState;

	// ====================================================================
	// FVector - 연산자 오버로딩이 있는 3D 벡터
	// ====================================================================
	lua.new_usertype<FVector>("Vector",
		// 생성자
		sol::constructors<FVector(), FVector(float, float, float)>(),

		// Properties
		"x", &FVector::X,
		"y", &FVector::Y,
		"z", &FVector::Z,

		// Operators
		sol::meta_function::addition, [](const FVector& a, const FVector& b) -> FVector {
			return FVector(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
		},
		sol::meta_function::subtraction, [](const FVector& a, const FVector& b) -> FVector {
			return FVector(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
		},
		sol::meta_function::multiplication, sol::overload(
			[](const FVector& v, float f) -> FVector { return v * f; },
			[](float f, const FVector& v) -> FVector { return v * f; }
		),

		// Methods
		"Length", &FVector::Length,
		"Normalize", &FVector::Normalize,
		"Dot", [](const FVector& a, const FVector& b) { return a.Dot(b); },
		"Cross", [](const FVector& a, const FVector& b) { return a.Cross(b); }
	);

	// ====================================================================
	// FQuaternion - Rotation representation
	// ====================================================================
	lua.new_usertype<FQuaternion>("Quaternion",
		sol::constructors<FQuaternion(), FQuaternion(float, float, float, float)>(),
		"x", &FQuaternion::X,
		"y", &FQuaternion::Y,
		"z", &FQuaternion::Z,
		"w", &FQuaternion::W
	);

	// ====================================================================
	// AActor - Base actor class
	// ====================================================================
	lua.new_usertype<AActor>("Actor",
		// Properties
		"UUID", sol::property(&AActor::GetUUID),
		"Location", sol::property(&AActor::GetActorLocation, &AActor::SetActorLocation),
		"Rotation", sol::property(&AActor::GetActorRotation, &AActor::SetActorRotation),

		// Methods
		"GetName", [](AActor* self) { return self->GetName().ToString(); },
		"GetLocation", &AActor::GetActorLocation,
		"SetLocation", &AActor::SetActorLocation,
		"GetRotation", &AActor::GetActorRotation,
		"SetRotation", &AActor::SetActorRotation,
		"PrintLocation", [](AActor* self) {
			FVector loc = self->GetActorLocation();
			UE_LOG("Actor %s Location: (%.2f, %.2f, %.2f)",
				self->GetName().ToString().c_str(), loc.X, loc.Y, loc.Z);
		}
	);

	UE_LOG_INFO("Lua core types registered (Vector, Quaternion, Actor)");
}

void UScriptManager::RegisterGlobalFunctions()
{
	sol::state& lua = *LuaState;

	// Override Lua print to use engine console
	lua["print"] = [](sol::variadic_args args) {
		FString output;
		for (auto arg : args)
		{
			if (output.length() > 0)
				output += " ";
			output += sol::object(arg).as<std::string>();
		}
		UE_LOG("%s", output.c_str());
	};

	// Engine log function
	lua["ULog"] = [](const std::string& msg) {
		UE_LOG("%s", msg.c_str());
	};

	// Get delta time
	lua["GetDeltaTime"] = []() -> float {
		return UTimeManager::GetInstance().GetDeltaTime();
	};

	// Get total time
	lua["GetTime"] = []() -> float {
		return UTimeManager::GetInstance().GetGameTime();
	};

	UE_LOG_INFO("Lua global functions registered");
}
