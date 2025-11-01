#pragma once
#include "Physics/Public/BoundingVolume.h"
#include "Global/Vector.h"
#include "Global/Quaternion.h"

struct FAABB;
struct FBoundingSphere;

/**
 * @brief Capsule 바운딩 볼륨
 * 실린더 중심 부분 + 양 끝 반구로 구성
 * HalfHeight는 실린더 부분의 절반 높이 (반구 제외)
 */
struct FBoundingCapsule : public IBoundingVolume
{
	FVector Center;
	float HalfHeight;  // 실린더 부분만의 절반 높이 (반구 제외)
	float Radius;
	FQuaternion Orientation;

	FBoundingCapsule();
	FBoundingCapsule(const FVector& InCenter, float InHalfHeight, float InRadius);
	FBoundingCapsule(const FVector& InCenter, float InHalfHeight, float InRadius, const FQuaternion& InOrientation);

	// IBoundingVolume interface
	bool RaycastHit() const override;
	void Update(const FMatrix& WorldMatrix) override;
	EBoundingVolumeType GetType() const override { return EBoundingVolumeType::Capsule; }

	// 충돌 감지
	bool Intersects(const FBoundingCapsule& Other) const;
	bool Intersects(const FBoundingSphere& Other) const;
	bool Intersects(const FAABB& Other) const;

	// 헬퍼 함수
	FVector GetTopSphereCenter() const;
	FVector GetBottomSphereCenter() const;

	// 캡슐의 중심축 (상단 → 하단 방향)
	FVector GetAxis() const;
};
