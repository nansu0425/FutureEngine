#pragma once

struct FAABB;
struct FBoundingSphere;
struct FBoundingCapsule;

/**
 * @brief 충돌 감지 유틸리티 함수 모음
 */
namespace CollisionDetection
{
	/**
	 * @brief AABB vs AABB 충돌 체크
	 */
	bool CheckOverlap(const FAABB& A, const FAABB& B);

	/**
	 * @brief Sphere vs Sphere 충돌 체크
	 */
	bool CheckOverlap(const FBoundingSphere& A, const FBoundingSphere& B);

	/**
	 * @brief AABB vs Sphere 충돌 체크
	 */
	bool CheckOverlap(const FAABB& Box, const FBoundingSphere& Sphere);

	/**
	 * @brief Sphere vs AABB 충돌 체크 (대칭)
	 */
	inline bool CheckOverlap(const FBoundingSphere& Sphere, const FAABB& Box)
	{
		return CheckOverlap(Box, Sphere);
	}

	/**
	 * @brief Capsule vs Capsule 충돌 체크
	 */
	bool CheckOverlap(const FBoundingCapsule& A, const FBoundingCapsule& B);

	/**
	 * @brief Capsule vs Sphere 충돌 체크
	 */
	bool CheckOverlap(const FBoundingCapsule& Capsule, const FBoundingSphere& Sphere);

	/**
	 * @brief Sphere vs Capsule 충돌 체크 (대칭)
	 */
	inline bool CheckOverlap(const FBoundingSphere& Sphere, const FBoundingCapsule& Capsule)
	{
		return CheckOverlap(Capsule, Sphere);
	}

	/**
	 * @brief Capsule vs AABB 충돌 체크
	 */
	bool CheckOverlap(const FBoundingCapsule& Capsule, const FAABB& Box);

	/**
	 * @brief AABB vs Capsule 충돌 체크 (대칭)
	 */
	inline bool CheckOverlap(const FAABB& Box, const FBoundingCapsule& Capsule)
	{
		return CheckOverlap(Capsule, Box);
	}
}
