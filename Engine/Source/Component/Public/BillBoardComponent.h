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

	void FaceCamera(
		const FVector& CameraPosition,
		const FVector& CameraUp,
		const FVector& FallbackUp
	);

	const TPair<FName, ID3D11ShaderResourceView*>& GetSprite() const;
	void SetSprite(const TPair<FName, ID3D11ShaderResourceView*>& Sprite);

	ID3D11SamplerState* GetSampler() const;

	UClass* GetSpecificWidgetClass() const override;

	static const FRenderState& GetClassDefaultRenderState(); 

private:
	TPair<FName, ID3D11ShaderResourceView*> Sprite = {"None", nullptr};
	ID3D11SamplerState* Sampler = nullptr;
};