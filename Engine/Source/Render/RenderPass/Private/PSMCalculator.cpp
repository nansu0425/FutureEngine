#include "Render/RenderPass/Public/PSMCalculator.h"
#include "Editor/Public/Camera.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>

#define W_EPSILON 0.001f
#define Z_EPSILON 0.0001f

static const FVector ZUp(0, 0, 1);  // FutureEngine Z-Up
static const FVector XForward(1, 0, 0);  // FutureEngine X-Forward

//-----------------------------------------------------------------------------
// Main Entry Point
//-----------------------------------------------------------------------------

void FPSMCalculator::CalculateShadowProjection(
	EShadowProjectionMode Mode,
	FMatrix& OutViewMatrix,
	FMatrix& OutProjectionMatrix,
	const FVector& LightDirection,
	UCamera* Camera,
	const std::vector<UStaticMeshComponent*>& Meshes,
	FPSMParameters& InOutParams)
{
	// Classify shadow casters and receivers
	std::vector<FPSMBoundingBox> ShadowCasters, ShadowReceivers;
	ComputeVirtualCameraParameters(
		LightDirection, Camera, Meshes,
		ShadowCasters, ShadowReceivers, InOutParams
	);

	// Build projection matrix based on mode
	switch (Mode)
	{
	case EShadowProjectionMode::Uniform:
		BuildUniformShadowMap(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	case EShadowProjectionMode::PSM:
		BuildPSMProjection(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	case EShadowProjectionMode::LSPSM:
		BuildLSPSMProjection(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	case EShadowProjectionMode::TSM:
		BuildTSMProjection(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;

	default:
		BuildUniformShadowMap(OutViewMatrix, OutProjectionMatrix, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, InOutParams);
		break;
	}
}

//-----------------------------------------------------------------------------
// Shadow Caster/Receiver Classification
//-----------------------------------------------------------------------------

void FPSMCalculator::ComputeVirtualCameraParameters(
	const FVector& LightDirection,
	UCamera* Camera,
	const std::vector<UStaticMeshComponent*>& Meshes,
	std::vector<FPSMBoundingBox>& OutShadowCasters,
	std::vector<FPSMBoundingBox>& OutShadowReceivers,
	FPSMParameters& InOutParams)
{
	OutShadowCasters.clear();
	OutShadowReceivers.clear();

	if (!Camera)
		return;

	// Get camera matrices
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix ViewMatrix = CamConstants.View;
	FMatrix ProjMatrix = CamConstants.Projection;
	FMatrix ViewProj = ViewMatrix * ProjMatrix;

	// Create frustum for culling
	FPSMFrustum SceneFrustum(ViewProj);

	// Light sweep direction (opposite of light direction)
	FVector SweepDir = -LightDirection.GetNormalized();

	// Test each mesh
	for (auto* Mesh : Meshes)
	{
		if (!Mesh || !Mesh->IsVisible())
			continue;

		// Get mesh world AABB
		FPSMBoundingBox MeshBox;
		GetMeshWorldBoundingBox(MeshBox, Mesh);

		if (!MeshBox.IsValid())
			continue;

		// Test against frustum
		int FrustumTest = SceneFrustum.TestBox(MeshBox);

		// Transform to view space for storage
		FPSMBoundingBox ViewSpaceBox;
		TransformBoundingBox(ViewSpaceBox, MeshBox, ViewMatrix);

		switch (FrustumTest)
		{
		case 0:  // Outside frustum - test swept sphere for shadow casting
		{
			FPSMBoundingSphere MeshSphere(MeshBox);
			if (SceneFrustum.TestSweptSphere(MeshSphere, SweepDir))
			{
				OutShadowCasters.push_back(ViewSpaceBox);
			}
			break;
		}

		case 1:  // Fully inside - both caster and receiver
			OutShadowCasters.push_back(ViewSpaceBox);
			OutShadowReceivers.push_back(ViewSpaceBox);
			break;

		case 2:  // Intersecting - both caster and receiver
			OutShadowCasters.push_back(ViewSpaceBox);
			OutShadowReceivers.push_back(ViewSpaceBox);
			break;
		}
	}

	// Compute near/far from receivers
	if (!OutShadowReceivers.empty())
	{
		float MinZ = FLT_MAX;
		float MaxZ = -FLT_MAX;

		for (const auto& Box : OutShadowReceivers)
		{
			MinZ = std::min(MinZ, Box.MinPt.Z);
			MaxZ = std::max(MaxZ, Box.MaxPt.Z);
		}

		InOutParams.ZNear = std::max(0.1f, MinZ);
		InOutParams.ZFar = std::min(10000.0f, MaxZ);
	}
	else
	{
		InOutParams.ZNear = 0.1f;
		InOutParams.ZFar = 10000.0f;
	}

	InOutParams.SlideBack = 0.0f;

	// Compute gamma (angle between light and view direction)
	FVector ViewDir(ViewMatrix.Data[0][2], ViewMatrix.Data[1][2], ViewMatrix.Data[2][2]);  // View forward (Z in view space)
	InOutParams.CosGamma = LightDirection.GetNormalized().Dot(ViewDir);
}

//-----------------------------------------------------------------------------
// Uniform Shadow Map (Orthographic)
//-----------------------------------------------------------------------------

void FPSMCalculator::BuildUniformShadowMap(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const std::vector<FPSMBoundingBox>& ShadowCasters,
	const std::vector<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix CameraView = CamConstants.View;

	// Get view space light direction
	FVector ViewLightDir = CameraView.TransformVector(LightDirection.GetNormalized());

	// Compute scene AABB
	FPSMBoundingBox SceneBox;
	if (Params.bUnitCubeClip)
	{
		SceneBox = FPSMBoundingBox(ShadowReceivers);
	}
	else
	{
		// Use camera frustum bounds
		float Aspect = Camera->GetAspect();
		float FOV = Camera->GetFovY();
		float FarDist = Camera->GetFarZ();
		float Height = std::tan(FOV * 0.5f) * FarDist;
		float Width = Height * Aspect;

		SceneBox.MinPt = FVector(-Width, -Height, 0.1f);
		SceneBox.MaxPt = FVector(Width, Height, FarDist);
	}

	// Light position (far away from scene center)
	FVector SceneCenter = SceneBox.GetCenter();
	float SceneRadius = (SceneBox.MaxPt - SceneBox.MinPt).Length() * 0.5f;

	FVector LightPos = SceneCenter + ViewLightDir * (SceneRadius + 50.0f);

	// Choose up vector
	FVector Up = ZUp;
	if (std::abs(ViewLightDir.Z) > 0.99f)
		Up = XForward;

	// Create light view matrix
	OutView = FMatrix::CreateLookAtLH(LightPos, SceneCenter, Up);

	// Transform scene AABB to light space
	FPSMBoundingBox LightSpaceBox;
	TransformBoundingBox(LightSpaceBox, SceneBox, OutView);

	// Also consider shadow casters
	if (!ShadowCasters.empty())
	{
		FPSMBoundingBox CasterBox(ShadowCasters);
		FPSMBoundingBox LightSpaceCasterBox;
		TransformBoundingBox(LightSpaceCasterBox, CasterBox, OutView);

		// Extend box to include casters
		LightSpaceBox.Merge(LightSpaceCasterBox.MinPt);
		LightSpaceBox.Merge(LightSpaceCasterBox.MaxPt);
	}

	// Create orthographic projection
	OutProj = FMatrix::CreateOrthoOffCenterLH(
		LightSpaceBox.MinPt.X, LightSpaceBox.MaxPt.X,
		LightSpaceBox.MinPt.Y, LightSpaceBox.MaxPt.Y,
		LightSpaceBox.MinPt.Z, LightSpaceBox.MaxPt.Z
	);

	// Combine with camera view
	OutView = CameraView * OutView;

	Params.PPNear = LightSpaceBox.MinPt.Z;
	Params.PPFar = LightSpaceBox.MaxPt.Z;
}

//-----------------------------------------------------------------------------
// PSM (Perspective Shadow Map)
//-----------------------------------------------------------------------------

void FPSMCalculator::BuildPSMProjection(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const std::vector<FPSMBoundingBox>& ShadowCasters,
	const std::vector<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix CameraView = CamConstants.View;
	FVector ViewLightDir = CameraView.TransformVector(LightDirection.GetNormalized());

	// Step 1: Setup virtual camera with slide-back
	FMatrix VirtualCameraView = FMatrix::Identity();
	FMatrix VirtualCameraProj;

	float ZNear = Params.ZNear;
	float ZFar = Params.ZFar;
	float SlideBack = 0.0f;

	if (Params.bSlideBackEnabled)
	{
		// Compute infinity plane distance
		float Infinity = ZFar / (ZFar - ZNear);
		float MinInfZ = Params.MinInfinityZ;

		if (Infinity <= MinInfZ)
		{
			float AdditionalSlide = MinInfZ * (ZFar - ZNear) - ZFar + Z_EPSILON;
			SlideBack = AdditionalSlide;
			ZFar += AdditionalSlide;
			ZNear += AdditionalSlide;
		}

		VirtualCameraView = FMatrix::CreateTranslation(FVector(0, 0, SlideBack));

		if (Params.bUnitCubeClip && !ShadowReceivers.empty())
		{
			// Compute tight FOV using bounding cone
			FVector EyePos = FVector::ZeroVector();
			FVector EyeDir = FVector(0, 0, 1);  // Z-Forward in view space (DirectX standard)
			FPSMBoundingCone BC(ShadowReceivers, VirtualCameraView, EyePos, EyeDir);

			float Width = 2.0f * std::tan(BC.FovX) * ZNear;
			float Height = 2.0f * std::tan(BC.FovY) * ZNear;
			VirtualCameraProj = FMatrix::CreatePerspectiveLH(Width, Height, ZNear, ZFar);
		}
		else
		{
			// Use camera FOV with slide-back adjustment
			float FOV = Camera->GetFovY();
			float Aspect = Camera->GetAspect();
			float ViewHeight = std::tan(FOV * 0.5f) * Camera->GetFarZ();
			float ViewWidth = ViewHeight * Aspect;
			float FarDist = Camera->GetFarZ();

			float HalfFovY = std::atan(ViewHeight / (FarDist + SlideBack));
			float HalfFovX = std::atan(ViewWidth / (FarDist + SlideBack));

			float Width = 2.0f * std::tan(HalfFovX) * ZNear;
			float Height = 2.0f * std::tan(HalfFovY) * ZNear;
			VirtualCameraProj = FMatrix::CreatePerspectiveLH(Width, Height, ZNear, ZFar);
		}
	}
	else
	{
		float FOV = Camera->GetFovY();
		float Aspect = Camera->GetAspect();
		VirtualCameraProj = FMatrix::CreatePerspectiveFovLH(FOV, Aspect, ZNear, ZFar);
	}

	FMatrix VirtualCameraViewProj = CameraView * VirtualCameraView * VirtualCameraProj;
	FMatrix EyeToPostProjective = VirtualCameraView * VirtualCameraProj;

	// Step 2: Transform light direction to post-projective space
	FVector4 LightDirW(ViewLightDir.X, ViewLightDir.Y, ViewLightDir.Z, 0.0f);
	FVector4 PPLight = VirtualCameraProj.TransformVector4(LightDirW);

	Params.bShadowTestInverted = (PPLight.W < 0.0f);

	// Step 3: Determine projection type
	if (std::abs(PPLight.W) <= W_EPSILON)
	{
		// Light is at infinity - use orthographic
		BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, Params);
		return;
	}

	// Step 4: Perspective PSM
	FVector PPLightPos(
		PPLight.X / PPLight.W,
		PPLight.Y / PPLight.W,
		PPLight.Z / PPLight.W
	);

	FMatrix LightView, LightProj;

	const FVector PPCubeCenter(0.0f, 0.0f, 0.5f);

	if (Params.bShadowTestInverted)
	{
		// Inverse projection (light behind camera)
		FPSMBoundingCone ViewCone;
		if (!Params.bUnitCubeClip)
		{
			// Project entire unit cube
			std::vector<FPSMBoundingBox> UnitBox;
			FPSMBoundingBox Cube;
			Cube.MinPt = FVector(-1, -1, 0);
			Cube.MaxPt = FVector(1, 1, 1);
			UnitBox.push_back(Cube);

			ViewCone = FPSMBoundingCone(UnitBox, FMatrix::Identity(), PPLightPos);
		}
		else
		{
			ViewCone = FPSMBoundingCone(ShadowReceivers, EyeToPostProjective, PPLightPos);
		}

		ViewCone.Near = std::max(0.001f, ViewCone.Near * 0.3f);
		Params.PPNear = -ViewCone.Near;
		Params.PPFar = ViewCone.Near;

		LightView = ViewCone.LookAtMatrix;

		float Width = 2.0f * std::tan(ViewCone.FovX) * Params.PPNear;
		float Height = 2.0f * std::tan(ViewCone.FovY) * Params.PPNear;
		LightProj = FMatrix::CreatePerspectiveLH(Width, Height, Params.PPNear, Params.PPFar);
	}
	else
	{
		// Regular projection
		FVector LookAt = PPCubeCenter - PPLightPos;
		float Distance = LookAt.Length();
		LookAt = LookAt * (1.0f / Distance);

		FVector Up = ZUp;
		if (std::abs(ZUp.Dot(LookAt)) > 0.99f)
			Up = XForward;

		if (!Params.bUnitCubeClip)
		{
			// Simple sphere-based FOV
			const float PPCubeRadius = 1.5f;

			LightView = FMatrix::CreateLookAtLH(PPLightPos, PPCubeCenter, Up);

			float Fovy = 2.0f * std::atan(PPCubeRadius / Distance);
			float Aspect = 1.0f;
			float FNear = std::max(0.001f, Distance - 2.0f * PPCubeRadius);
			float FFar = Distance + 2.0f * PPCubeRadius;

			Params.PPNear = FNear;
			Params.PPFar = FFar;

			LightProj = FMatrix::CreatePerspectiveFovLH(Fovy, Aspect, FNear, FFar);
		}
		else
		{
			// Unit cube clipping
			FPSMBoundingCone BC(ShadowReceivers, EyeToPostProjective, PPLightPos);

			LightView = BC.LookAtMatrix;

			float Fovy = 2.0f * BC.FovY;
			float Aspect = BC.FovX / BC.FovY;
			float FNear = BC.Near * 0.6f;  // Slight adjustment
			float FFar = BC.Far;

			FNear = std::max(0.001f, FNear);

			Params.PPNear = FNear;
			Params.PPFar = FFar;

			LightProj = FMatrix::CreatePerspectiveFovLH(Fovy, Aspect, FNear, FFar);
		}
	}

	// Step 5: Combine matrices
	FMatrix LightViewProj = LightView * LightProj;
	OutView = VirtualCameraViewProj;
	OutProj = LightViewProj;

	// NOTE: In actual rendering, these are combined: FinalMatrix = View * VirtualView * VirtualProj * LightView * LightProj
	// But for ShadowMapPass, we return the final combined matrix as ViewProj
	OutView = CameraView;
	OutProj = VirtualCameraView * VirtualCameraProj * LightView * LightProj;

	Params.SlideBack = SlideBack;
}

//-----------------------------------------------------------------------------
// LSPSM & TSM - Placeholder implementations
// These are complex and can be added later if needed
//-----------------------------------------------------------------------------

void FPSMCalculator::BuildLSPSMProjection(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const std::vector<FPSMBoundingBox>& ShadowCasters,
	const std::vector<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	// Fallback to uniform for now
	BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera, ShadowCasters, ShadowReceivers, Params);
}

void FPSMCalculator::BuildTSMProjection(
	FMatrix& OutView,
	FMatrix& OutProj,
	const FVector& LightDirection,
	UCamera* Camera,
	const std::vector<FPSMBoundingBox>& ShadowCasters,
	const std::vector<FPSMBoundingBox>& ShadowReceivers,
	FPSMParameters& Params)
{
	// Fallback to uniform for now
	BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera, ShadowCasters, ShadowReceivers, Params);
}
