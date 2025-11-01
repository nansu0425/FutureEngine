#include "pch.h"
#include "Manager/Script/Public/ScriptManager.h"


// 엔진 인클루드
#include "Global/Vector.h"
#include "Global/Quaternion.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/SceneComponent.h"
#include "Component/Public/ScriptComponent.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Manager/Path/Public/PathManager.h"
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
sol::table UScriptManager::GetTable(const FString& ScriptPath)
{
	auto it = LuaScriptMap.find(ScriptPath);
	if (it != LuaScriptMap.end())
	{
		return LuaScriptMap[ScriptPath].GlobalTable;
	}
	return LuaState->create_table();
}
bool UScriptManager::IsLoadedScript(const FString& ScriptPath)
{
	auto it = LuaScriptMap.find(ScriptPath);
	if (it != LuaScriptMap.end())
	{
		return true;
	}
	return false;
}

bool UScriptManager::LoadLuaScript(const FString& ScriptPath)
{
	// 파일 수정 시간 기록 (Engine 경로 우선)
	UPathManager& PathMgr = UPathManager::GetInstance();
	path EngineScriptPath = PathMgr.GetEngineDataPath() / "Scripts" / ScriptPath.c_str();
	path BuildScriptPath = PathMgr.GetDataPath() / "Scripts" / ScriptPath.c_str();

	path FullPath;
	if (std::filesystem::exists(EngineScriptPath))
	{
		FullPath = EngineScriptPath;
	}
	else if (std::filesystem::exists(BuildScriptPath))
	{
		FullPath = BuildScriptPath;
	}
	else
	{
		UE_LOG_WARNING("ScriptManager: 스크립트 파일을 찾을 수 없어 Hot Reload 등록 실패: %s", ScriptPath.c_str());
		return false;
	}

	std::error_code ErrorCode;
	auto LastWriteTime = std::filesystem::last_write_time(FullPath, ErrorCode);
	if (!ErrorCode)
	{
		UE_LOG_INFO("루아 스크립트 로드 시작 - %s", ScriptPath.c_str());

		try
		{
			sol::table GlobalTable = LuaState->script_file(FullPath.string());
			LuaScriptMap[ScriptPath].Path = ScriptPath;
			LuaScriptMap[ScriptPath].LastCompileTime = LastWriteTime;
			LuaScriptMap[ScriptPath].GlobalTable = GlobalTable;
		}
		catch (const sol::error& e)
		{
			UE_LOG_ERROR("Lua compile/load error: %s", e.what());
			return false;
		}

		UE_LOG_INFO("ScriptManager: Hot Reload 등록 완료 - %s", ScriptPath.c_str());
		return true;
	}
	else
	{
		UE_LOG_WARNING("ScriptManager: 스크립트 수정 시간 조회 실패: %s (%s)", ScriptPath.c_str(), ErrorCode.message().c_str());
		return false;
	}
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

	// Vector 생성자 함수 등록 (호출 가능하게)
	lua.set_function("Vector", sol::overload(
		[]() { return FVector(0.0f, 0.0f, 0.0f); },
		[](float x, float y, float z) { return FVector(x, y, z); }
	));

	// FVector usertype 등록 (메서드와 프로퍼티)
	lua.new_usertype<FVector>("FVector",
		sol::no_constructor,  // 생성자는 위에서 Vector 함수로 등록했음

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

	// Quaternion 생성자 함수 등록 (호출 가능하게)
	lua.set_function("Quaternion", sol::overload(
		[]() { return FQuaternion(); },
		[](float x, float y, float z, float w) { return FQuaternion(x, y, z, w); }
	));

	// FQuaternion usertype 등록 (메서드와 프로퍼티)
	lua.new_usertype<FQuaternion>("FQuaternion",
		sol::no_constructor,  // 생성자는 위에서 Quaternion 함수로 등록했음
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

			sol::object obj = arg;
			sol::type argType = obj.get_type();

			// 타입별 처리
			if (argType == sol::type::number)
			{
				// 정수 또는 실수 처리
				if (obj.is<int32>())
					output += std::to_string(obj.as<int32>());
				else if (obj.is<double>())
					output += std::to_string(obj.as<double>());
				else
					output += std::to_string(obj.as<float>());
			}
			else if (argType == sol::type::string)
			{
				output += obj.as<std::string>();
			}
			else if (argType == sol::type::boolean)
			{
				output += (obj.as<bool>() ? "true" : "false");
			}
			else if (argType == sol::type::nil)
			{
				output += "nil";
			}
			else if (argType == sol::type::table)
			{
				output += "[table]";
			}
			else if (argType == sol::type::function)
			{
				output += "[function]";
			}
			else if (argType == sol::type::userdata)
			{
				// FVector 처리
				if (obj.is<FVector>())
				{
					FVector vec = obj.as<FVector>();
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "(%.3f, %.3f, %.3f)", vec.X, vec.Y, vec.Z);
					output += buffer;
				}
				// FQuaternion 처리
				else if (obj.is<FQuaternion>())
				{
					FQuaternion quat = obj.as<FQuaternion>();
					char buffer[128];
					snprintf(buffer, sizeof(buffer), "(%.3f, %.3f, %.3f, %.3f)", quat.X, quat.Y, quat.Z, quat.W);
					output += buffer;
				}
				else
				{
					output += "[userdata]";
				}
			}
			else
			{
				output += "[unknown]";
			}
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

/*-----------------------------------------------------------------------------
	Hot Reload System
-----------------------------------------------------------------------------*/

TSet<FString> UScriptManager::GatherHotReloadTargets()
{
	TSet<FString> HotReloadTargets;

	UPathManager& PathMgr = UPathManager::GetInstance();

	for (const auto& Pair : LuaScriptMap)
	{
		const FString& ScriptPath = Pair.first;
		const auto& CachedLastWriteTime = Pair.second.LastCompileTime;

		// Engine 경로 우선 확인
		path EngineScriptPath = PathMgr.GetEngineDataPath() / "Scripts" / ScriptPath.c_str();
		path BuildScriptPath = PathMgr.GetDataPath() / "Scripts" / ScriptPath.c_str();

		path FullPath;
		if (std::filesystem::exists(EngineScriptPath))
		{
			FullPath = EngineScriptPath;
		}
		else if (std::filesystem::exists(BuildScriptPath))
		{
			FullPath = BuildScriptPath;
		}
		else
		{
			continue;
		}

		// 현재 수정 시간 조회
		std::error_code ErrorCode;
		auto CurrentLastWriteTime = std::filesystem::last_write_time(FullPath, ErrorCode);
		if (ErrorCode)
		{
			continue;
		}

		// 시간 비교
		if (CurrentLastWriteTime != CachedLastWriteTime)
		{
			HotReloadTargets.insert(ScriptPath);
			// 수정 시간 업데이트
		}
	}

	return HotReloadTargets;
}

void UScriptManager::HotReloadScripts()
{
	// Hot Reload 체크 주기 제한 (성능 최적화)
	float CurrentTime = UTimeManager::GetInstance().GetGameTime();
	float TimeSinceLastCheck = CurrentTime - LastHotReloadCheckTime;

	if (TimeSinceLastCheck < HotReloadCheckInterval)
	{
		// 아직 체크 주기가 안 됨
		return;
	}

	// 체크 시간 업데이트
	LastHotReloadCheckTime = CurrentTime;

	TSet<FString> HotReloadTargets = GatherHotReloadTargets();

	if (HotReloadTargets.empty())
	{
		return;
	}

	UE_LOG_INFO("ScriptManager: Hot Reloading %d script(s)...", static_cast<int32>(HotReloadTargets.size()));

	for (const FString& ScriptPath : HotReloadTargets)
	{
		auto It = LuaScriptMap.find(ScriptPath);
		if (It == LuaScriptMap.end())
		{
			continue;
		}

		LoadLuaScript(ScriptPath);
	}
}
