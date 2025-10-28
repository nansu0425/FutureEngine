#pragma once
#include "Core/Public/Object.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/Grid.h"
#include "Editor/Public/ObjectPicker.h"
#include "Editor/Public/BatchLines.h"
#include "editor/Public/Camera.h"

class UPrimitiveComponent;
class UUUIDTextComponent;
class FViewportClient;
class UCamera;
class ULevel;
class USplitterWidget;
struct FRay;

enum class EViewportLayoutState
{
	Multi,
	Single,
	Animating,
};

class UEditor : public UObject
{
	DECLARE_CLASS(UEditor, UObject)
public:
	UEditor();
	~UEditor();

	void Update();
	void RenderEditorGeometry();
	void Collect2DRender(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);
	void RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport);

	void SetViewMode(EViewModeIndex InNewViewMode) { CurrentViewMode = InNewViewMode; }
	EViewModeIndex GetViewMode() const { return CurrentViewMode; }

	// 레이아웃 제어는 ViewportManager가 담당

	void SelectActor(AActor* InActor);
	AActor* GetSelectedActor() const { return SelectedActor; }
	void SelectComponent(UActorComponent* InComponent);
	void SelectActorAndComponent(AActor* InActor, UActorComponent* InComponent);
	void FocusOnSelectedActor();
	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }

	// Alt + 드래그 복사 기능
	UActorComponent* DuplicateComponent(UActorComponent* InSourceComponent, AActor* InParentActor);
	AActor* DuplicateActor(AActor* InSourceActor);

// Getter
public:
	UBatchLines* GetBatchLines() { return &BatchLines; }
	UGizmo* GetGizmo() { return &Gizmo; }

	
private:
	void UpdateBatchLines();
	void ProcessMouseInput();
	
	// 모든 기즈모 드래그 함수가 ActiveCamera를 받도록 통일
	FVector GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay);
	FQuaternion GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay);
	FVector GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay);

	inline float Lerp(const float A, const float B, const float Alpha)
	{
		return A * (1 - Alpha) + B * Alpha;
	}

	// Focus Target Calculation
	bool GetComponentFocusTarget(UActorComponent* Component, FVector& OutCenter, float& OutRadius);
	bool GetActorFocusTarget(AActor* Actor, FVector& OutCenter, float& OutRadius);

	UObjectPicker ObjectPicker;
	AActor* SelectedActor = nullptr; // 선택된 액터
	UActorComponent* SelectedComponent = nullptr; // 선택된 컴포넌트

	// 선택 타입 (Actor vs Component)
	bool bIsActorSelected = true; // true: Actor 선택 (Root Component), false: Component 선택

	// Alt + 드래그 복사 모드
	bool bIsInCopyMode = false; // Alt 키 누른 상태로 드래그 시작 시 true
	AActor* CopiedActor = nullptr; // 복사된 Actor (복사 모드 시)
	UActorComponent* CopiedComponent = nullptr; // 복사된 Component (복사 모드 시)

	UCamera* Camera;
	UGizmo Gizmo;
	UBatchLines BatchLines;

	// InteractionViewport 제거: ViewportManager가 레이아웃 관리

	EViewModeIndex CurrentViewMode = EViewModeIndex::VMI_BlinnPhong;

	int32 ActiveViewportIndex = 0;

	// 드래그 중 뷰포트 고정 처리를 위한 트래킹
	int32 LockedViewportIndexForDrag = -1;
	bool bWasRightMouseDown = false;

	// 최소 스케일 값 설정
	static constexpr float MIN_SCALE_VALUE = 0.01f;

	// Camera focus animation
	bool bIsCameraAnimating = false;
	float CameraAnimationTime = 0.0f;
	ECameraType AnimatingCameraType = ECameraType::ECT_Perspective;
	TArray<FVector> CameraStartLocation;
	TArray<FVector> CameraStartRotation;
	TArray<FVector> CameraTargetLocation;
	TArray<FVector> CameraTargetRotation;
	static constexpr float CAMERA_ANIMATION_DURATION = 0.3f;
	void UpdateCameraAnimation();
};
