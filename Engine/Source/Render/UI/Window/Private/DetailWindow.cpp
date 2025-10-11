#include "pch.h"
#include "Render/UI/Window/Public/DetailWindow.h"

#include "Render/UI/Widget/Public/ActorDetailWidget.h"
#include "Render/UI/Widget/Public/ActorTerminationWidget.h"
#include "Render/UI/Widget/Public/TargetActorTransformWidget.h"
#include "Render/UI/Widget/Public/DecalTextureSelectionWidget.h"

#include "Manager/UI/Public/UIManager.h"
#include "Level/Public/Level.h"

/**
 * @brief Detail Window Constructor
 * Selected된 Actor의 관리를 위한 적절한 크기의 윈도우 제공
 */
UDetailWindow::UDetailWindow()
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Details";
	Config.DefaultSize = ImVec2(330, 440);
	Config.DefaultPosition = ImVec2(1565, 590);
	Config.MinSize = ImVec2(250, 300);
	Config.DockDirection = EUIDockDirection::Right;
	Config.Priority = 20;
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;

	Config.UpdateWindowFlags();
	SetConfig(Config);

	UActorDetailWidget* ActorDetailWidget = NewObject<UActorDetailWidget>();
	AddWidget(ActorDetailWidget);
	AddWidget(NewObject<UTargetActorTransformWidget>());
	UActorTerminationWidget* ActorTerminationWidget = NewObject<UActorTerminationWidget>();
	ActorTerminationWidget->SetActorDetailWidget(ActorDetailWidget);
	AddWidget(ActorTerminationWidget);
}

/**
 * @brief 초기화 함수
 */
void UDetailWindow::Initialize()
{
	UE_LOG("DetailWindow: Initialized");
}

// @brief 새로운 Actor가 피킹된 경우 소유한 컴포넌트 전용 Widget을 표시한다
void UDetailWindow::OnSelectedComponentChanged(UActorComponent* Component)
{
	DeleteWidget(ComponentSpecificWidget);
	ComponentSpecificWidget = nullptr;
	
	if (Component)
	{
		UClass* WidgetClass = Component->GetSpecificWidgetClass();
		if (WidgetClass)
		{
			UWidget* NewWidget = Cast<UWidget>(NewObject(WidgetClass));
			if (NewWidget)
			{
				AddWidget(NewWidget);
				ComponentSpecificWidget = NewWidget;
			}
		}
	}
}
