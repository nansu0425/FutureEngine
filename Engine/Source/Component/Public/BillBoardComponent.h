#pragma once

#include "Component/Public/PrimitiveComponent.h"

UCLASS()
class UBillBoardComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UBillBoardComponent, UPrimitiveComponent)

public:
	UBillBoardComponent();
	~UBillBoardComponent();

	void FaceCamera(const FVector& CameraForward);

	UTexture* GetSprite() const;
	void SetSprite(UTexture* Sprite);

	UClass* GetSpecificWidgetClass() const override;

	static const FRenderState& GetClassDefaultRenderState(); 

private:
	UTexture* Sprite;
};