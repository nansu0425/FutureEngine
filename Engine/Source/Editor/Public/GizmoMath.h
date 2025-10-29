#pragma once
#include "GizmoTypes.h"
#include "Global/Vector.h"

class UCamera;

/**
 * @brief Gizmo 수학 계산 유틸리티 구조체
 * - Screen-space 스케일 계산
 * - Quarter Ring 방향 계산
 * - 색상 계산 (하이라이트 등)
 * - 축 인덱스 변환
 */
struct FGizmoMath
{
	/**
	 * @brief Screen-space uniform scale 계산
	 * @param InCamera 카메라
	 * @param InViewport 뷰포트 정보
	 * @param InGizmoLocation 기즈모 위치
	 * @param InDesiredPixelSize 원하는 화면 픽셀 크기
	 * @return 계산된 스케일 값
	 */
	static float CalculateScreenSpaceScale(const UCamera* InCamera, const D3D11_VIEWPORT& InViewport,
	                                       const FVector& InGizmoLocation, float InDesiredPixelSize);

	/**
	 * @brief Quarter Ring의 시작/끝 방향 계산 (카메라 정렬용)
	 * @param InCamera 카메라
	 * @param InAxis 회전 축
	 * @param InGizmoLocation 기즈모 위치
	 * @param OutStartDir 시작 방향 (출력)
	 * @param OutEndDir 끝 방향 (출력)
	 */
	static void CalculateQuarterRingDirections(UCamera* InCamera, EGizmoDirection InAxis,
	                                           const FVector& InGizmoLocation,
	                                           FVector& OutStartDir, FVector& OutEndDir);

	/**
	 * @brief 방향에 대한 색상 계산 (하이라이트 포함)
	 * @param InDirection 기즈모 방향
	 * @param InCurrentDirection 현재 선택된 방향
	 * @param InIsDragging 드래그 중인지 여부
	 * @param InGizmoColors 기본 기즈모 색상 배열 (RGB 순서)
	 * @return 계산된 색상
	 */
	static FVector4 CalculateColor(EGizmoDirection InDirection, EGizmoDirection InCurrentDirection,
	                               bool InIsDragging, const TArray<FVector4>& InGizmoColors);

	/**
	 * @brief 기즈모 방향을 축 인덱스로 변환
	 * @param InDirection 기즈모 방향
	 * @return 축 인덱스 (0=X, 1=Y, 2=Z)
	 */
	static int32 DirectionToAxisIndex(EGizmoDirection InDirection);

	/**
	 * @brief 방향이 평면인지 확인
	 * @param InDirection 기즈모 방향
	 * @return 평면 여부
	 */
	static bool IsPlaneDirection(EGizmoDirection InDirection);

	/**
	 * @brief 기즈모 축 벡터 반환
	 * @param InDirection 기즈모 방향
	 * @return 축 벡터
	 */
	static FVector GetAxisVector(EGizmoDirection InDirection);

	/**
	 * @brief 평면의 법선 벡터 반환
	 * @param InDirection 기즈모 방향 (XY_Plane, XZ_Plane, YZ_Plane)
	 * @return 법선 벡터
	 */
	static FVector GetPlaneNormal(EGizmoDirection InDirection);

	/**
	 * @brief 평면의 두 접선 벡터 반환
	 * @param InDirection 기즈모 방향 (XY_Plane, XZ_Plane, YZ_Plane)
	 * @param OutTangent1 첫 번째 접선 (출력)
	 * @param OutTangent2 두 번째 접선 (출력)
	 */
	static void GetPlaneTangents(EGizmoDirection InDirection, FVector& OutTangent1, FVector& OutTangent2);
};
