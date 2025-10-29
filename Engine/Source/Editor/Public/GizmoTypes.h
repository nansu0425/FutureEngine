#pragma once

enum class EGizmoMode
{
	Translate,
	Rotate,
	Scale
};

enum class EGizmoDirection
{
	Right,
	Up,
	Forward,
	XY_Plane,  // Z축 수직 평면 (X, Y 두 축 동시 제어)
	XZ_Plane,  // Y축 수직 평면 (X, Z 두 축 동시 제어)
	YZ_Plane,  // X축 수직 평면 (Y, Z 두 축 동시 제어)
	Center,    // 중심점 (Translation: 카메라 평면 드래그, Scale: 균일 스케일)
	None
};

struct FGizmoTranslationCollisionConfig
{
	FGizmoTranslationCollisionConfig() = default;

	float Radius = 0.04f;
	float Height = 0.9f;
	float Scale = 2.f;
};

struct FGizmoRotateCollisionConfig
{
	FGizmoRotateCollisionConfig() = default;

	float OuterRadius = 1.0f;  // 링 큰 반지름
	float InnerRadius = 0.89f;  // 링 안쪽 반지름
	float Thickness = 0.04f;   // 링의 3D 두께
	float Scale = 2.0f;
};

/**
 * @brief Gizmo 상수 정의 구조체
 */
struct FGizmoConstants
{
	// Quarter ring mesh generation
	static constexpr int32 QuarterRingSegments = 48;
	static constexpr float QuarterRingStartAngle = 0.0f;
	static constexpr float QuarterRingEndAngle = PI / 2.0f;  // 90 degrees

	// Gizmo axis definitions (X ring: ZAxis × YAxis, Y ring: XAxis × ZAxis, Z ring: XAxis × YAxis)
	static inline const FVector LocalAxis0[3] = {
		FVector(0.0f, 0.0f, 1.0f),  // X ring: Z axis
		FVector(1.0f, 0.0f, 0.0f),  // Y ring: X axis
		FVector(1.0f, 0.0f, 0.0f)   // Z ring: X axis
	};

	static inline const FVector LocalAxis1[3] = {
		FVector(0.0f, 1.0f, 0.0f),  // X ring: Y axis
		FVector(0.0f, 0.0f, 1.0f),  // Y ring: Z axis
		FVector(0.0f, 1.0f, 0.0f)   // Z ring: Y axis
	};
};
