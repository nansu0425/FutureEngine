#pragma once

class UPrimitiveComponent;
class AActor;

/**
 * @brief 두 컴포넌트 간의 겹침(Overlap) 정보를 저장하는 구조체
 */
struct FOverlapInfo
{
	UPrimitiveComponent* OverlappingComponent = nullptr;

	FOverlapInfo() = default;

	FOverlapInfo(UPrimitiveComponent* InComponent)
		: OverlappingComponent(InComponent)
	{}

	bool operator==(const FOverlapInfo& Other) const
	{
		return OverlappingComponent == Other.OverlappingComponent;
	}

	bool operator!=(const FOverlapInfo& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * @brief 충돌(Hit) 결과를 저장하는 구조체 (향후 확장용)
 */
struct FHitResult
{
	UPrimitiveComponent* HitComponent = nullptr;
	AActor* HitActor = nullptr;
	FVector ImpactPoint = FVector(0.f, 0.f, 0.f);
	FVector ImpactNormal = FVector(0.f, 0.f, 1.f);

	FHitResult() = default;
};
