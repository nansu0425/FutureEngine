#pragma once
#include "Widget.h"

class UClass;
class UBillBoardComponent;

UClass;
class USpriteSelectionWidget
	: public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(USpriteSelectionWidget, UWidget)
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Special Member Function
	USpriteSelectionWidget();
	~USpriteSelectionWidget() override;

private:
	void SetSpriteOfActor(FString NewSprite);

	AActor* SelectedActor = nullptr;
	UBillBoardComponent* SelectedBillBoard = nullptr;

	inline static uint32 WidgetNum = 0;
};
