#pragma once
#include "Widget.h"

class UClass;
class UTextComponent;

class USetTextComponentWidget
	:public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(USetTextComponentWidget, UWidget)
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;

	// Special Member Function
	USetTextComponentWidget();
	~USetTextComponentWidget() override;

	void UpdateTextFromActor();

private:
	AActor* SelectedActor = nullptr;
	UTextComponent* SelectedTextComponent = nullptr;

	inline static uint32 WidgetNum = 0;
};