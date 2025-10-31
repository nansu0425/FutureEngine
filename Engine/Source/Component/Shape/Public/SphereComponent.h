#pragma once
#include "Component/Shape/Public/ShapeComponent.h"

struct FBoundingSphere;
class UBoxComponent;
class UCapsuleComponent;

/**
 * @brief Sphere 충돌 형상을 나타내는 컴포넌트
 * FBoundingSphere 기반 충돌 감지
 */
UCLASS()
class USphereComponent : public UShapeComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USphereComponent, UShapeComponent)

public:
	USphereComponent();
	virtual ~USphereComponent() override;

	// Sphere Radius
	float GetSphereRadius() const { return SphereRadius; }
	void SetSphereRadius(float InRadius);

	// World space BoundingSphere 얻기
	FBoundingSphere GetWorldSphere() const;

	// 충돌 체크 오버라이드
	virtual bool CheckOverlapWith(const UPrimitiveComponent* Other) const override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	// 구의 반지름
	float SphereRadius = 50.0f;

private:
	// 개별 Shape와의 충돌 체크
	bool CheckOverlapWithBox(const UBoxComponent* OtherBox) const;
	bool CheckOverlapWithSphere(const USphereComponent* OtherSphere) const;
	bool CheckOverlapWithCapsule(const UCapsuleComponent* OtherCapsule) const;
};
