#pragma once
#include "Actor/Public/Actor.h"

// 전방 선언
class UScriptComponent;
class UStaticMeshComponent;

/**
 * AScriptTestActor
 *
 * Lua 스크립팅 통합 테스트용 Actor
 * Lua 스크립트를 실행하는 UScriptComponent를 포함합니다.
 */
UCLASS()
class AScriptTestActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(AScriptTestActor, AActor)

private:
	UScriptComponent* ScriptComp;
	UStaticMeshComponent* MeshComponent;

public:
	AScriptTestActor();
	virtual ~AScriptTestActor() override = default;

	/**
	 * 스크립트 컴포넌트 가져오기
	 */
	UScriptComponent* GetScriptComponent() const { return ScriptComp; }

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
