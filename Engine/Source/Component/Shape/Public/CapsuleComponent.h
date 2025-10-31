#pragma once
#include "Component/Shape/Public/ShapeComponent.h"
#include "Physics/Public/BoundingCapsule.h"

class UBoxComponent;
class USphereComponent;

/**
 * @brief Capsule 충돌 형상을 나타내는 컴포넌트
 * FBoundingCapsule 기반 충돌 감지
 * 실린더 + 양 끝 반구 형태
 */
UCLASS()
class UCapsuleComponent : public UShapeComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

public:
	UCapsuleComponent();
	virtual ~UCapsuleComponent() override;

	// Capsule Parameters
	float GetCapsuleHalfHeight() const { return CapsuleHalfHeight; }
	void SetCapsuleHalfHeight(float InHalfHeight);

	float GetCapsuleRadius() const { return CapsuleRadius; }
	void SetCapsuleRadius(float InRadius);

	// World space Capsule 얻기
	FBoundingCapsule GetWorldCapsule() const;

	// IBoundingVolume 인터페이스
	virtual const IBoundingVolume* GetBoundingVolume() override;

	// 충돌 체크 오버라이드
	virtual bool CheckOverlapWith(const UPrimitiveComponent* Other) const override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	// 실린더 부분의 절반 높이 (반구 제외)
	float CapsuleHalfHeight = 50.0f;

	// 캡슐의 반지름
	float CapsuleRadius = 25.0f;

	// Cached bounding volume for GetBoundingVolume()
	mutable FBoundingCapsule CachedWorldCapsule;

private:
	// 개별 Shape와의 충돌 체크
	bool CheckOverlapWithBox(const UBoxComponent* OtherBox) const;
	bool CheckOverlapWithSphere(const USphereComponent* OtherSphere) const;
	bool CheckOverlapWithCapsule(const UCapsuleComponent* OtherCapsule) const;
};
