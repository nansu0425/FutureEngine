#include "pch.h"
#include "Editor/Public/GizmoMath.h"

#include "Editor/Public/GizmoTypes.h"
#include "Editor/Public/Camera.h"

float FGizmoMath::CalculateScreenSpaceScale(const UCamera* InCamera, const D3D11_VIEWPORT& InViewport,
                                             const FVector& InGizmoLocation, float InDesiredPixelSize)
{
	if (!InCamera)
	{
		return 1.0f;
	}

	const FCameraConstants& CameraConstants = InCamera->GetFViewProjConstants();
	const FMatrix& ProjMatrix = CameraConstants.Projection;
	const ECameraType CameraType = InCamera->GetCameraType();
	const float ViewportHeight = InViewport.Height;

	if (ViewportHeight < 1.0f)
	{
		return 1.0f;
	}

	float ProjYY = abs(ProjMatrix.Data[1][1]);
	if (ProjYY < 0.0001f)
	{
		return 1.0f;
	}

	float Scale;

	if (CameraType == ECameraType::ECT_Perspective)
	{
		const FMatrix& ViewMatrix = CameraConstants.View;

		FVector4 GizmoPos4(InGizmoLocation.X, InGizmoLocation.Y, InGizmoLocation.Z, 1.0f);
		FVector4 ViewSpacePos = GizmoPos4 * ViewMatrix;

		float ProjectedDepth = abs(ViewSpacePos.Z);

		constexpr float MinDepth = 1.0f;
		ProjectedDepth = max(ProjectedDepth, MinDepth);

		Scale = (InDesiredPixelSize * ProjectedDepth) / (ProjYY * ViewportHeight * 0.5f);
	}
	else
	{
		float OrthoHeight = 2.0f / ProjYY;
		Scale = (InDesiredPixelSize * OrthoHeight) / ViewportHeight;
	}

	Scale = max(0.01f, min(Scale, 100.0f));

	return Scale;
}

void FGizmoMath::CalculateQuarterRingDirections(UCamera* InCamera, EGizmoDirection InAxis,
                                                 const FVector& InGizmoLocation,
                                                 FVector& OutStartDir, FVector& OutEndDir)
{
	if (!InCamera)
	{
		OutStartDir = FVector::ForwardVector();
		OutEndDir = FVector::RightVector();
		return;
	}

	const int Idx = DirectionToAxisIndex(InAxis);
	const FVector CameraLoc = InCamera->GetLocation();
	const FVector DirectionToWidget = (InGizmoLocation - CameraLoc).GetNormalized();

	FVector Axis0 = FGizmoConstants::LocalAxis0[Idx];
	FVector Axis1 = FGizmoConstants::LocalAxis1[Idx];

	const bool bMirrorAxis0 = (Axis0.Dot(DirectionToWidget) <= 0.0f);
	const bool bMirrorAxis1 = (Axis1.Dot(DirectionToWidget) <= 0.0f);
	const FVector RenderAxis0 = bMirrorAxis0 ? Axis0 : -Axis0;
	const FVector RenderAxis1 = bMirrorAxis1 ? Axis1 : -Axis1;

	FVector LocalRenderAxis0 = RenderAxis0;
	FVector LocalRenderAxis1 = RenderAxis1;
	LocalRenderAxis0.Normalize();
	LocalRenderAxis1.Normalize();

	OutStartDir = LocalRenderAxis0;
	OutEndDir = LocalRenderAxis1;
}

FVector4 FGizmoMath::CalculateColor(EGizmoDirection InDirection, EGizmoDirection InCurrentDirection,
                                     bool InIsDragging, const TArray<FVector4>& InGizmoColors)
{
	const int Idx = DirectionToAxisIndex(InDirection);
	const FVector4& BaseColor = InGizmoColors[Idx];
	bool bIsHighlight = (InDirection == InCurrentDirection);

	bool bIsInAxisPlane = IsPlaneDirection(InDirection);

	if (!bIsHighlight && IsPlaneDirection(InCurrentDirection) && !bIsInAxisPlane)
	{
		if (InCurrentDirection == EGizmoDirection::XY_Plane)
		{
			bIsHighlight = (InDirection == EGizmoDirection::Forward || InDirection == EGizmoDirection::Right);
		}
		else if (InCurrentDirection == EGizmoDirection::XZ_Plane)
		{
			bIsHighlight = (InDirection == EGizmoDirection::Forward || InDirection == EGizmoDirection::Up);
		}
		else if (InCurrentDirection == EGizmoDirection::YZ_Plane)
		{
			bIsHighlight = (InDirection == EGizmoDirection::Right || InDirection == EGizmoDirection::Up);
		}
	}

	if (InIsDragging && bIsHighlight)
	{
		return {0.8f, 0.8f, 0.0f, 1.0f};
	}
	else if (bIsHighlight)
	{
		return {1.0f, 1.0f, 0.0f, 1.0f};
	}
	else
	{
		return BaseColor;
	}
}

int32 FGizmoMath::DirectionToAxisIndex(EGizmoDirection InDirection)
{
	switch (InDirection)
	{
	case EGizmoDirection::Forward:    return 0;
	case EGizmoDirection::Right:      return 1;
	case EGizmoDirection::Up:         return 2;
	case EGizmoDirection::XY_Plane:   return 2;  // Z축
	case EGizmoDirection::XZ_Plane:   return 1;  // Y축
	case EGizmoDirection::YZ_Plane:   return 0;  // X축
	default:                          return 2;
	}
}

bool FGizmoMath::IsPlaneDirection(EGizmoDirection InDirection)
{
	return InDirection == EGizmoDirection::XY_Plane ||
	       InDirection == EGizmoDirection::XZ_Plane ||
	       InDirection == EGizmoDirection::YZ_Plane;
}

FVector FGizmoMath::GetAxisVector(EGizmoDirection InDirection)
{
	switch (InDirection)
	{
	case EGizmoDirection::Forward:    return {1, 0, 0};
	case EGizmoDirection::Right:      return {0, 1, 0};
	case EGizmoDirection::Up:         return {0, 0, 1};
	default:                          return {0, 1, 0};
	}
}

FVector FGizmoMath::GetPlaneNormal(EGizmoDirection InDirection)
{
	switch (InDirection)
	{
	case EGizmoDirection::XY_Plane:   return {0, 0, 1};  // Z축
	case EGizmoDirection::XZ_Plane:   return {0, 1, 0};  // Y축
	case EGizmoDirection::YZ_Plane:   return {1, 0, 0};  // X축
	default:                          return {0, 0, 1};
	}
}

void FGizmoMath::GetPlaneTangents(EGizmoDirection InDirection, FVector& OutTangent1, FVector& OutTangent2)
{
	switch (InDirection)
	{
	case EGizmoDirection::XY_Plane:
		OutTangent1 = FVector(1, 0, 0);  // X축
		OutTangent2 = FVector(0, 1, 0);  // Y축
		break;
	case EGizmoDirection::XZ_Plane:
		OutTangent1 = FVector(1, 0, 0);  // X축
		OutTangent2 = FVector(0, 0, 1);  // Z축
		break;
	case EGizmoDirection::YZ_Plane:
		OutTangent1 = FVector(0, 1, 0);  // Y축
		OutTangent2 = FVector(0, 0, 1);  // Z축
		break;
	default:
		OutTangent1 = FVector(1, 0, 0);
		OutTangent2 = FVector(0, 1, 0);
		break;
	}
}
