#include "pch.h"
#include "Render/UI/Widget/Public/SetTextComponentWidget.h"

#include "Level/Public/Level.h"
#include "Actor/Public/CubeActor.h"
#include "Actor/Public/SphereActor.h"
#include "Actor/Public/SquareActor.h"
#include "Actor/Public/TriangleActor.h"
#include "Actor/Public/StaticMeshActor.h"
#include "Actor/Public/BillBoardActor.h"
#include "Actor/Public/TextActor.h"
#include "Component/Public/TextComponent.h"

#include <climits>

IMPLEMENT_CLASS(USetTextComponentWidget, UWidget)

USetTextComponentWidget::USetTextComponentWidget()
	: UWidget("Set TextComponent Widget")
{
}

USetTextComponentWidget::~USetTextComponentWidget() = default;

void USetTextComponentWidget::Initialize()
{
	// Do Nothing Here
}

void USetTextComponentWidget::Update()
{
	// 매 프레임 Level의 선택된 Actor를 확인해서 정보 반영
	// TODO(KHJ): 적절한 위치를 찾을 것
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (CurrentLevel)
	{
		AActor* NewSelectedActor = GEditor->GetEditorModule()->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != NewSelectedActor)
		{
			SelectedActor = NewSelectedActor;
		}

		// Get Current Selected Actor Information
		if (SelectedActor)
			UpdateTextFromActor();
	}
}

void USetTextComponentWidget::RenderWidget()
{
	if (!SelectedActor)
		return;

	ImGui::Separator();
	ImGui::Text("Type Text");

	ImGui::Spacing();

	// 버퍼를 직접 관리해야 함
	
	static char buf[256] = "";
	const FString& TextOfComponent = SelectedTextComponent->GetText();
	memcpy(buf, TextOfComponent.c_str(), std::min(sizeof(buf), TextOfComponent.size()));
	FString TagName = FString("TypeText##") + std::to_string(WidgetNum);

	if (ImGui::InputText(TagName.c_str(), buf, IM_ARRAYSIZE(buf)))
	{
		SelectedTextComponent->SetText(FString(buf));
	}
	
	ImGui::Separator();

	WidgetNum = (WidgetNum + 1) % std::numeric_limits<uint32>::max();
}

void USetTextComponentWidget::UpdateTextFromActor()
{
	for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
	{
		UTextComponent* TextComp = Cast<UTextComponent>(Comp);
		if (TextComp)
			SelectedTextComponent = TextComp;
	}
}
