#pragma once
#include "Component/Public/DecalComponent.h"



UCLASS()
class UDecalSpotLightComponent : public UDecalComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UDecalSpotLightComponent, UDecalComponent)

public:
	UDecalSpotLightComponent();
	~UDecalSpotLightComponent();

	

public:
	void TickComponent(float DeltaTime) override;

	void UpdateProjectionMatrix() override;

	virtual UObject* Duplicate() override;


private:
	


};

