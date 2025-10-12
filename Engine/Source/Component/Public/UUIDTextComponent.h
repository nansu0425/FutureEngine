#pragma once
#include "Component/Public/TextComponent.h"
#include "Global/Matrix.h"

class AActor;

UCLASS()
class UUUIDTextComponent : public UTextComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UUUIDTextComponent, UTextComponent)
public:
	UUUIDTextComponent();
	~UUUIDTextComponent() override;

	virtual void OnSelected() override;
	virtual void OnDeselected() override;

	void FaceCamera(const FVector& InCameraForward) override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	UClass* GetSpecificWidgetClass() const override;
};