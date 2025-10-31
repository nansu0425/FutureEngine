#pragma once
#include "Core/Public/Object.h"

class UScriptManager;

//코루틴을 등록하고 UCoroutineManager 에서 등록된 코루틴들의 조건을 검사 후 조건을 만족하면 해당 객체의 resume을 호출 해주는 방향으로 제작하기로함
//코루틴을 등록할 때 필요한 값들은 호출한 객체 (ScriptComponent) 와 해당 코루틴 안에서 사용되는 지역변수값들을 모두 저장하고 있어야 한다...
//일단 루아에서 c++의 함수를 호출하는 방법부터 작업 해보자

UCLASS()
class UCoroutineManager : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UCoroutineManager, UObject)

	

};