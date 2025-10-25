#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/Camera.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Actor/Public/Actor.h"
#include "Editor/Public/Editor.h"
#include "Global/Quaternion.h"

UGizmo::UGizmo()
{
	UAssetManager& ResourceManager = UAssetManager::GetInstance();
	Primitives.resize(3);
	GizmoColor.resize(3);

	/* *
	* @brief 0: Forward(x), 1: Right(y), 2: Up(z)
	*/
	GizmoColor[0] = FVector4(1, 0, 0, 1);
	GizmoColor[1] = FVector4(0, 1, 0, 1);
	GizmoColor[2] = FVector4(0, 0, 1, 1);

	/* *
	* @brief Translation Setting
	*/
	const float ScaleT = TranslateCollisionConfig.Scale;
	Primitives[0].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
	Primitives[0].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
	Primitives[0].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[0].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[0].bShouldAlwaysVisible = true;

	/* *
	* @brief Rotation Setting
	*/
	Primitives[1].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Ring);
	Primitives[1].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Ring);
	Primitives[1].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[1].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[1].bShouldAlwaysVisible = true;

	/* *
	* @brief Scale Setting
	*/
	Primitives[2].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::CubeArrow);
	Primitives[2].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::CubeArrow);
	Primitives[2].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[2].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[2].bShouldAlwaysVisible = true;

	/* *
	* @brief Render State
	*/
	RenderState.FillMode = EFillMode::Solid;
	RenderState.CullMode = ECullMode::None;
}

UGizmo::~UGizmo() = default;

void UGizmo::UpdateScale(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	TargetComponent = Cast<USceneComponent>(GEditor->GetEditorModule()->GetSelectedComponent());
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// 스크린에서 균일한 사이즈를 가지도록 하기 위한 스케일 조정
	const float Scale = CalculateScreenSpaceScale(InCamera, InViewport, 120.0f);

	TranslateCollisionConfig.Scale = Scale;
	RotateCollisionConfig.Scale = Scale;
}

void UGizmo::RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	TargetComponent = Cast<USceneComponent>(GEditor->GetEditorModule()->GetSelectedComponent());
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// 스크린에서 균일한 사이즈를 가지도록 하기 위한 스케일 조정
	const float RenderScale = CalculateScreenSpaceScale(InCamera, InViewport, 120.0f);

	URenderer& Renderer = URenderer::GetInstance();
	const int Mode = static_cast<int>(GizmoMode);
	auto& P = Primitives[Mode];
	P.Location = TargetComponent->GetWorldLocation();

	P.Scale = FVector(RenderScale, RenderScale, RenderScale);

	// 기즈모 기준 회전 결정 (World/Local 모드)
	FQuaternion BaseRot;
	if (GizmoMode == EGizmoMode::Rotate && !bIsWorld && bIsDragging)
	{
		// 로컬 회전 모드 드래그 중: 드래그 시작 시점의 회전 고정
		BaseRot = DragStartActorRotationQuat;
	}
	else if (GizmoMode == EGizmoMode::Scale)
	{
		// 스케일 모드: 항상 로컬 좌표
		BaseRot = TargetComponent->GetWorldRotationAsQuaternion();
	}
	else
	{
		// 평행이동/회전 모드: World는 Identity, Local은 현재 회전
		BaseRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	}

	// X축 (Forward) - 빨간색
	// Row-major: GizmoLocal * ObjectWorld
	P.Rotation = FQuaternion::Identity() * BaseRot;
	P.Color = ColorFor(EGizmoDirection::Forward);
	Renderer.RenderEditorPrimitive(P, RenderState);

	// Y축 (Right) - 초록색 (Z축 주위로 -90도 회전)
	P.Rotation = FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(-90.0f)) * BaseRot;
	P.Color = ColorFor(EGizmoDirection::Right);
	Renderer.RenderEditorPrimitive(P, RenderState);

	// Z축 (Up) - 파란색 (Y축 주위로 90도 회전)
	P.Rotation = FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(90.0f)) * BaseRot;
	P.Color = ColorFor(EGizmoDirection::Up);
	Renderer.RenderEditorPrimitive(P, RenderState);
}

void UGizmo::ChangeGizmoMode()
{
	switch (GizmoMode)
	{
	case EGizmoMode::Translate:
		GizmoMode = EGizmoMode::Rotate; break;
	case EGizmoMode::Rotate:
		GizmoMode = EGizmoMode::Scale; break;
	case EGizmoMode::Scale:
		GizmoMode = EGizmoMode::Translate;
	}
}

void UGizmo::SetLocation(const FVector& Location)
{
	TargetComponent->SetWorldLocation(Location);
}

bool UGizmo::IsInRadius(float Radius)
{
	if (Radius >= RotateCollisionConfig.InnerRadius * RotateCollisionConfig.Scale && Radius <= RotateCollisionConfig.OuterRadius * RotateCollisionConfig.Scale)
		return true;
	return false;
}

void UGizmo::OnMouseDragStart(FVector& CollisionPoint)
{
	bIsDragging = true;
	DragStartMouseLocation = CollisionPoint;

	if (TargetComponent)
	{
		DragStartActorLocation = TargetComponent->GetWorldLocation();
		DragStartActorRotation = TargetComponent->GetWorldRotation();
		DragStartActorRotationQuat = TargetComponent->GetWorldRotationAsQuaternion();
		DragStartActorScale = TargetComponent->GetWorldScale3D();
	}
}

// 하이라이트 색상은 렌더 시점에만 계산 (상태 오염 방지)
FVector4 UGizmo::ColorFor(EGizmoDirection InAxis) const
{
	const int Idx = AxisIndex(InAxis);
	const FVector4& BaseColor = GizmoColor[Idx];
	const bool bIsHighlight = (InAxis == GizmoDirection);

	if (bIsDragging && bIsHighlight)
	{
		// 드래그 중: 짙은 노란색 (Dark Yellow)
		return FVector4(0.8f, 0.8f, 0.0f, 1.0f);
	}
	else if (bIsHighlight)
	{
		// 호버 중: 밝은 노란색
		return FVector4(1.0f, 1.0f, 0.0f, 1.0f);
	}
	else
	{
		// 기본 색상 (빨강/초록/파랑)
		return BaseColor;
	}
}

float UGizmo::CalculateScreenSpaceScale(UCamera* InCamera, const D3D11_VIEWPORT& InViewport, float InDesiredPixelSize) const
{
	if (!InCamera || !TargetComponent)
	{
		return 1.0f;
	}

	// Unreal Engine Screen Space Scale 계산
	// Perspective: ViewZ 기반 (FOV + 화면 위치 자동 보정)
	// Orthographic: OrthoHeight 기반

	const FVector GizmoLocation = TargetComponent->GetWorldLocation();
	const FCameraConstants& CameraConstants = InCamera->GetFViewProjConstants();
	const FMatrix& ProjMatrix = CameraConstants.Projection;
	const ECameraType CameraType = InCamera->GetCameraType();
	const float ViewportHeight = InViewport.Height;

	if (ViewportHeight < 1.0f)
	{
		return 1.0f;
	}

	// ProjYY = Projection[1][1]
	// Perspective: cot(FOV_Y/2)
	// Orthographic: 2 / OrthoHeight
	float ProjYY = std::abs(ProjMatrix.Data[1][1]);
	if (ProjYY < 0.0001f)
	{
		return 1.0f;
	}

	float Scale = 1.0f;

	if (CameraType == ECameraType::ECT_Perspective)
	{
		// Perspective: Projected depth 사용 (Unreal Engine 방식)
		// Gizmo를 View space로 변환하여 실제 화면상의 depth를 계산

		const FMatrix& ViewMatrix = CameraConstants.View;

		// Gizmo 위치를 View space로 변환
		FVector4 GizmoPos4(GizmoLocation.X, GizmoLocation.Y, GizmoLocation.Z, 1.0f);
		FVector4 ViewSpacePos = GizmoPos4 * ViewMatrix;

		// View space Z (depth) = ViewSpacePos.Z
		// 이것이 실제 화면에 투영될 때의 depth
		float ProjectedDepth = std::abs(ViewSpacePos.Z);

		// 최소 거리 제한
		constexpr float MinDepth = 1.0f;
		if (ProjectedDepth < MinDepth)
		{
			ProjectedDepth = MinDepth;
		}

		// Unreal Engine 공식:
		// Scale = PixelSize * ProjectedDepth / (ProjYY * ViewportHeight * 0.5)
		// ProjectedDepth는 화면에 투영된 실제 depth이므로 측면에서 봐도 일정
		Scale = (InDesiredPixelSize * ProjectedDepth) / (ProjYY * ViewportHeight * 0.5f);
	}
	else // Orthographic
	{
		// Orthographic: OrthoHeight 기반 계산
		// Ortho Projection Matrix: ProjMatrix[1][1] = 2 / OrthoHeight
		// OrthoHeight = 2 / |ProjYY|
		float OrthoHeight = 2.0f / ProjYY;

		// Orthographic 공식:
		// 1 픽셀 = OrthoHeight / ViewportHeight
		// Scale = DesiredPixels * PixelToWorld
		Scale = (InDesiredPixelSize * OrthoHeight) / ViewportHeight;
	}

	// 스케일 범위 제한
	Scale = std::max(0.01f, std::min(Scale, 100.0f));

	return Scale;
}
