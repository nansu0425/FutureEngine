#pragma once
#include "Component/Shape/Public/ShapeComponent.h"
#include "Physics/Public/AABB.h"

class USphereComponent;
class UCapsuleComponent;

/**
 * @brief Box 충돌 형상을 나타내는 컴포넌트
 * AABB(Axis-Aligned Bounding Box) 기반 충돌 감지
 */
UCLASS()
class UBoxComponent : public UShapeComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UBoxComponent, UShapeComponent)

public:
	UBoxComponent();
	virtual ~UBoxComponent() override;

	// Box Extent (Half-size)
	FVector GetBoxExtent() const { return BoxExtent; }
	void SetBoxExtent(const FVector& InExtent);

	// World space AABB 얻기
	FAABB GetWorldAABB() const;

	// UPrimitiveComponent 오버라이드 - World AABB 얻기 (out 파라미터 버전)
	void GetWorldAABB(FVector& OutMin, FVector& OutMax);

	// IBoundingVolume 인터페이스
	virtual const IBoundingVolume* GetBoundingVolume() override;

	// 충돌 체크 오버라이드
	virtual bool CheckOverlapWith(const UPrimitiveComponent* Other) const override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// Duplication
	virtual UObject* Duplicate() override;

protected:
	// 박스의 반 크기 (Half-size)
	FVector BoxExtent = FVector(2.0f, 2.0f, 2.0f);

	// Cached bounding volume for GetBoundingVolume()
	mutable FAABB CachedWorldAABB;

private:
	// 개별 Shape와의 충돌 체크
	bool CheckOverlapWithBox(const UBoxComponent* OtherBox) const;
	bool CheckOverlapWithSphere(const USphereComponent* OtherSphere) const;
	bool CheckOverlapWithCapsule(const UCapsuleComponent* OtherCapsule) const;
};
