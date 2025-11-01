#include "pch.h"
#include "Manager/Script/Public/CoroutineManager.h"
#include "Manager/Script/Public/ScriptManager.h"
#include "Component/Public/ScriptComponent.h"

IMPLEMENT_SINGLETON_CLASS(UCoroutineManager, UObject)

UCoroutineManager::UCoroutineManager()
{

}
UCoroutineManager::~UCoroutineManager()
{

}

void UCoroutineManager::Init()
{
    RegisterStateFunc();
}

void UCoroutineManager::RegisterStateFunc()
{

    sol::state& LuaState = UScriptManager::GetInstance().GetLuaState();
    LuaState.set_function("WaitForSeconds", sol::overload(
        [](float InWaitTime) { return FWaitCondition(InWaitTime); }
    ));

    LuaState.set_function("WaitUntil",
        [](sol::function luaFunc) {
            return FWaitCondition([luaFunc]() -> bool {
                auto result = luaFunc();
                return result.valid() && result.get<bool>();
                });
        }
    );

    LuaState.set_function("WaitTick",
        []() {
            return FWaitCondition();
        }
    );

    LuaState.new_usertype<FWaitCondition>("FWaitCondition",
        sol::no_constructor,

        "WaitTime", &FWaitCondition::WaitTime,
        "WaitType", &FWaitCondition::WaitType
    );
}
void UCoroutineManager::RegisterEnvFunc(UScriptComponent* Comp, sol::environment& Env)
{
    //StartCoroutine
    Env["StartCoroutine"] = [Comp](const FString& FuncName)
    {
        UCoroutineManager::GetInstance().StartCoroutine(Comp, FuncName);
    };

    //StopCoroutine
    Env["StopCoroutine"] = [Comp](const FString& FuncName)
    {
        UCoroutineManager::GetInstance().StopCoroutine(Comp, FuncName);
    };


}
void UCoroutineManager::StartCoroutine(UScriptComponent* Comp, const FString& FuncName)
{
    sol::state& LuaState = UScriptManager::GetInstance().GetLuaState();
    sol::environment& Env = Comp->GetEnv();

	CoroutineData Data;
	Data.Comp = Comp;
    Data.Thread = sol::thread::create(LuaState);
    sol::function Func = Env[FuncName];
    if (!Func.valid()) {
        UE_LOG("Function not found %s", FuncName.c_str());
        return;
    }
    Data.Coroutine = sol::coroutine(Data.Thread.state(), Func);
    Data.GCRef = sol::make_reference(LuaState, Data.Thread);
    Data.FuncName = FName(FuncName);

    if (ResumeCoroutine(Data) == false)
    {
        Coroutines.push_back(Data);
    }
}
void UCoroutineManager::StopCoroutine(UScriptComponent* Comp, const FString& FuncName)
{
    FName Name = FName(FuncName);
    for (int i = Coroutines.size() - 1; i >= 0; --i)
    {
        if (Coroutines[i].Comp == Comp &&
            Coroutines[i].FuncName == Name)
        {
            Coroutines.erase(Coroutines.begin() + i);
        }
    }
}


void UCoroutineManager::Update(const float DeltaTime)
{
    int CoroutineCount = Coroutines.size();

    for (int i = Coroutines.size() - 1; i >= 0; --i)
    {
        CoroutineData& Data = Coroutines[i];
        bool bEnd = false;
        if (Data.WaitCondition.WaitType == EWaitType::Time) 
        {
            Data.WaitCondition.WaitTime -= DeltaTime;
            if (Data.WaitCondition.WaitTime <= 0)
            {
                Data.WaitCondition.WaitType = EWaitType::None;
                bEnd = ResumeCoroutine(Data);
            }
        }
        else if (Data.WaitCondition.WaitType == EWaitType::Lambda) {
            if (Data.WaitCondition.Lambda && Data.WaitCondition.Lambda()) {
                Data.WaitCondition.WaitType = EWaitType::None;
                bEnd = ResumeCoroutine(Data);
            }
        }
        else if (Data.WaitCondition.WaitType == EWaitType::None)
        {
            bEnd = ResumeCoroutine(Data);
        }


        if (bEnd)
        {
            Coroutines.erase(Coroutines.begin() + i);
        }
    }

}

//끝났는지 여부 리턴 true = 끝남
bool UCoroutineManager::ResumeCoroutine(CoroutineData& InData)
{
    FString name = InData.FuncName.ToString();
    auto YieldResult = InData.Coroutine();

    sol::thread_status status = InData.Thread.status();

    sol::call_status Status = InData.Coroutine.status();
    if (Status == sol::call_status::yielded)
    {
        if (YieldResult.return_count() > 0)
        {
            InData.WaitCondition = YieldResult[0];
            UE_LOG("Coroutine Resume : %s", InData.FuncName.ToString().c_str());
        }
    }
    else if (Status == sol::call_status::ok)
    {
        UE_LOG("Coroutine End : %s", InData.FuncName.ToString().c_str());

        return true;
    }
    else
    {
        sol::error err = YieldResult;
        UE_LOG("Coroutine Error : %s \n Reason : %s", InData.FuncName.ToString().c_str(), err.what());
        return true;
    }
    return false;
}
void UCoroutineManager::StopAllCoroutine(UScriptComponent* Comp)
{
    // 완료된 코루틴 제거
    Coroutines.erase(
        std::remove_if(Coroutines.begin(), Coroutines.end(),
            [Comp](CoroutineData& Data) { return Data.Comp == Comp; }),
        Coroutines.end()
    );
}