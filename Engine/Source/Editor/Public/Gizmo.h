#pragma once
#include "Editor/Public/EditorPrimitive.h"
#include "Core/Public/Object.h"
#include "Actor/Public/Actor.h"

class UCamera;

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
	float InnerRadius = 0.9f;  // 링 굵기 r
	float Scale = 2.0f;
};

class UGizmo :
	public UObject
{
	GENERATED_BODY()
	DECLARE_CLASS(UGizmo, UObject)
	
public:
	UGizmo();
	~UGizmo() override;
	void UpdateScale(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);
	void RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);
	void ChangeGizmoMode();
	void SetGizmoMode(EGizmoMode Mode) { GizmoMode = Mode; }

	/**
	 * @brief Setter
	 */
	void SetLocation(const FVector& Location);
	void SetGizmoDirection(EGizmoDirection Direction) { GizmoDirection = Direction; }
	void SetComponentRotation(const FQuaternion& Rotation) const { TargetComponent->SetWorldRotation(Rotation); }
	void SetComponentScale(const FVector& Scale) const { TargetComponent->SetWorldScale3D(Scale); }
	void SetPreviousMouseLocation(const FVector& Location) { PreviousMouseLocation = Location; }
	void SetCurrentRotationAngle(float Angle) { CurrentRotationAngle = Angle; }

	void SetWorld() { bIsWorld = true; }
	void SetLocal() { bIsWorld = false; }
	bool IsWorldMode() const { return bIsWorld; }

	/**
	 * @brief Getter
	 */
	float GetTranslateScale() const { return TranslateCollisionConfig.Scale; }
	float GetRotateScale() const { return RotateCollisionConfig.Scale; }
	EGizmoDirection GetGizmoDirection() const { return GizmoDirection; }
	FVector GetGizmoLocation() { return Primitives[static_cast<int>(GizmoMode)].Location; }
	FQuaternion GetComponentRotation() const { return TargetComponent->GetWorldRotationAsQuaternion(); }
	FVector GetComponentScale() const { return TargetComponent->GetWorldScale3D(); }
	FVector GetDragStartMouseLocation() { return DragStartMouseLocation; }
	FVector GetDragStartActorLocation() { return DragStartActorLocation; }
	FVector GetDragStartActorRotation() { return DragStartActorRotation; }
	FQuaternion GetDragStartActorRotationQuat() const { return DragStartActorRotationQuat; }
	FVector GetDragStartActorScale() { return DragStartActorScale; }
	EGizmoMode GetGizmoMode() const { return GizmoMode; }
	FVector GetGizmoAxis() const
	{
		FVector Axis[3]{ {1,0,0},{0,1,0},{0,0,1} };
		return Axis[AxisIndex(GizmoDirection)];
	}

	float GetTranslateRadius() const { return TranslateCollisionConfig.Radius * TranslateCollisionConfig.Scale; }
	float GetTranslateHeight() const { return TranslateCollisionConfig.Height * TranslateCollisionConfig.Scale; }
	float GetRotateOuterRadius() const { return RotateCollisionConfig.OuterRadius * RotateCollisionConfig.Scale; }
	float GetRotateInnerRadius() const { return RotateCollisionConfig.InnerRadius * RotateCollisionConfig.Scale; }
	float GetRotateThickness()   const { return std::max(0.001f, RotateCollisionConfig.InnerRadius * RotateCollisionConfig.Scale); }
	USceneComponent* GetTargetComponent() const { return TargetComponent; }
	void SetSelectedComponent(USceneComponent* InComponent) { TargetComponent = InComponent; }
	USceneComponent* GetSelectedComponent() const { return TargetComponent; }
	bool IsInRadius(float Radius);
	bool HasComponent() const { return TargetComponent; }
	FVector GetPreviousMouseLocation() const { return PreviousMouseLocation; }
	float GetCurrentRotationAngle() const { return CurrentRotationAngle; }
	float GetCurrentRotationAngleDegrees() const { return FVector::GetRadianToDegree(CurrentRotationAngle); }

	// Quarter ring camera alignment
	void CalculateQuarterRingDirections(UCamera* InCamera,
		EGizmoDirection InAxis, FVector& OutStartDir, FVector& OutEndDir) const;

	/**
	 * @brief 마우스 관련
	 */
	void EndDrag()
	{
		bIsDragging = false;
		CurrentRotationAngle = 0.0f;
	}
	bool IsDragging() const { return bIsDragging; }
	void OnMouseHovering() {}
	void OnMouseDragStart(FVector& CollisionPoint);
	void OnMouseRelease(EGizmoDirection DirectionReleased) {}

private:
	static int AxisIndex(EGizmoDirection InDirection)
	{
		switch (InDirection)
		{
		case EGizmoDirection::Forward:	return 0;
		case EGizmoDirection::Right:	return 1;
		case EGizmoDirection::Up:		return 2;
		default:						return 2;
		}
	}

	// 렌더 시 하이라이트 색상 계산 (상태 오염 방지)
	FVector4 ColorFor(EGizmoDirection InAxis) const;

	// Screen-space uniform scale calculation
	float CalculateScreenSpaceScale(UCamera* InCamera, const D3D11_VIEWPORT& InViewport, float InDesiredPixelSize) const;

	TArray<FEditorPrimitive> Primitives;
	USceneComponent* TargetComponent = nullptr;

	TArray<FVector4> GizmoColor;
	FVector DragStartActorLocation;
	FVector DragStartMouseLocation;
	FVector DragStartActorRotation;
	FQuaternion DragStartActorRotationQuat;
	FVector DragStartActorScale;

	FGizmoTranslationCollisionConfig TranslateCollisionConfig;
	FGizmoRotateCollisionConfig RotateCollisionConfig;
	bool bIsDragging = false;
	bool bIsWorld = true;	// Gizmo coordinate mode (true; world)

	FRenderState RenderState;

	EGizmoDirection GizmoDirection = EGizmoDirection::None;
	EGizmoMode      GizmoMode = EGizmoMode::Translate;

	// 회전 드래그 상태
	FVector PreviousMouseLocation;
	float CurrentRotationAngle = 0.0f;
};
