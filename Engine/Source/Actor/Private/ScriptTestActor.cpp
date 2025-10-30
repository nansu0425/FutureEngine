#include "pch.h"
#include "Actor/Public/ScriptTestActor.h"
#include "Component/Public/ScriptComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/SceneComponent.h"

IMPLEMENT_CLASS(AScriptTestActor, AActor)

AScriptTestActor::AScriptTestActor()
{
	// Tick 활성화 (PIE/Game 모드에서 Tick 호출)
	bCanEverTick = true;

	// 에디터에서도 Tick 활성화 (에디터 모드에서도 움직이게 하려면)
	bTickInEditor = true;

	// 루트 컴포넌트 생성
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>("Root");
	SetRootComponent(Root);

	// 메시 컴포넌트 생성
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	MeshComponent->AttachToComponent(GetRootComponent());

	// 스크립트 컴포넌트 생성
	ScriptComp = CreateDefaultSubobject<UScriptComponent>("Script");

	// 기본 스크립트를 template.lua로 설정
	ScriptComp->SetScriptPath("template.lua");
}

void AScriptTestActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// ScriptComp 컴포넌트 직렬화는 컴포넌트 자체에서 처리됩니다
}
