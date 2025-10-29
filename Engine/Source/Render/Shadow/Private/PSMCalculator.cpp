#include "pch.h"
#include "Render/Shadow/Public/PSMCalculator.h"
#include "Editor/Public/Camera.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include <algorithm>
#include <cmath>

#define W_EPSILON 0.001f
#define Z_EPSILON 0.0001f

static const FVector ZUp(0, 0, 1);  // FutureEngine Z-Up
static const FVector XForward(1, 0, 0);  // FutureEngine X-Forward

//-----------------------------------------------------------------------------
// 메인 진입점
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
	// 그림자 캐스터와 리시버 분류
	std::vector<FPSMBoundingBox> ShadowCasters, ShadowReceivers;
	ComputeVirtualCameraParameters(
		LightDirection, Camera, Meshes,
		ShadowCasters, ShadowReceivers, InOutParams
	);

	// 모드에 따라 투영 행렬 생성
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
// 그림자 캐스터/리시버 분류
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

	// 카메라 행렬 가져오기
	const FCameraConstants& CamConstants = Camera->GetFViewProjConstants();
	FMatrix ViewMatrix = CamConstants.View;
	FMatrix ProjMatrix = CamConstants.Projection;
	FMatrix ViewProj = ViewMatrix * ProjMatrix;

	// 컬링용 프러스텀 생성
	FPSMFrustum SceneFrustum(ViewProj);

	// 빛 스윕 방향 (빛 방향의 반대)
	FVector SweepDir = -LightDirection.GetNormalized();

	// 각 메시 테스트
	for (auto* Mesh : Meshes)
	{
		if (!Mesh || !Mesh->IsVisible())
			continue;

		// 메시 월드 AABB 가져오기
		FPSMBoundingBox MeshBox;
		GetMeshWorldBoundingBox(MeshBox, Mesh);

		if (!MeshBox.IsValid())
			continue;

		// 프러스텀에 대해 테스트
		int FrustumTest = SceneFrustum.TestBox(MeshBox);

		// 저장을 위해 뷰 공간으로 변환
		FPSMBoundingBox ViewSpaceBox;
		TransformBoundingBox(ViewSpaceBox, MeshBox, ViewMatrix);

		switch (FrustumTest)
		{
		case 0:  // 프러스텀 밖 - 그림자 투사를 위해 swept sphere 테스트
		{
			FPSMBoundingSphere MeshSphere(MeshBox);
			if (SceneFrustum.TestSweptSphere(MeshSphere, SweepDir))
			{
				OutShadowCasters.push_back(ViewSpaceBox);
			}
			break;
		}

		case 1:  // 완전히 내부 - 캐스터이자 리시버
			OutShadowCasters.push_back(ViewSpaceBox);
			OutShadowReceivers.push_back(ViewSpaceBox);
			break;

		case 2:  // 교차 - 캐스터이자 리시버
			OutShadowCasters.push_back(ViewSpaceBox);
			OutShadowReceivers.push_back(ViewSpaceBox);
			break;
		}
	}

	// 리시버로부터 near/far 계산
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

	// 감마 계산 (빛과 뷰 방향 사이의 각도)
	FVector ViewDir(ViewMatrix.Data[0][2], ViewMatrix.Data[1][2], ViewMatrix.Data[2][2]);  // 뷰 전방 (뷰 공간의 Z)
	InOutParams.CosGamma = LightDirection.GetNormalized().Dot(ViewDir);
}

//-----------------------------------------------------------------------------
// Uniform 그림자 맵 (직교 투영)
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

	// 뷰 공간 빛 방향 가져오기
	FVector ViewLightDir = CameraView.TransformVector(LightDirection.GetNormalized());

	// 씬 AABB 계산
	FPSMBoundingBox SceneBox;
	if (Params.bUnitCubeClip)
	{
		SceneBox = FPSMBoundingBox(ShadowReceivers);
	}
	else
	{
		// 카메라 프러스텀 범위 사용
		float Aspect = Camera->GetAspect();
		float FOV = Camera->GetFovY();
		float FarDist = Camera->GetFarZ();
		float Height = std::tan(FOV * 0.5f) * FarDist;
		float Width = Height * Aspect;

		SceneBox.MinPt = FVector(-Width, -Height, 0.1f);
		SceneBox.MaxPt = FVector(Width, Height, FarDist);
	}

	// 빛 위치 (씬 중심에서 멀리, 빛의 반대 방향)
	FVector SceneCenter = SceneBox.GetCenter();
	float SceneRadius = (SceneBox.MaxPt - SceneBox.MinPt).Length() * 0.5f;

	// 빛은 비추는 방향의 반대쪽에 위치
	FVector LightPos = SceneCenter - ViewLightDir * (SceneRadius + 50.0f);

	// 위쪽 벡터 선택
	FVector Up = ZUp;
	if (std::abs(ViewLightDir.Z) > 0.99f)
		Up = XForward;

	// 라이트 뷰 행렬 생성
	OutView = FMatrix::CreateLookAtLH(LightPos, SceneCenter, Up);

	// 씬 AABB를 라이트 공간으로 변환
	FPSMBoundingBox LightSpaceBox;
	TransformBoundingBox(LightSpaceBox, SceneBox, OutView);

	// 그림자 캐스터도 고려
	if (!ShadowCasters.empty())
	{
		FPSMBoundingBox CasterBox(ShadowCasters);
		FPSMBoundingBox LightSpaceCasterBox;
		TransformBoundingBox(LightSpaceCasterBox, CasterBox, OutView);

		// 캐스터를 포함하도록 박스 확장
		LightSpaceBox.Merge(LightSpaceCasterBox.MinPt);
		LightSpaceBox.Merge(LightSpaceCasterBox.MaxPt);
	}

	// 직교 투영 생성
	OutProj = FMatrix::CreateOrthoOffCenterLH(
		LightSpaceBox.MinPt.X, LightSpaceBox.MaxPt.X,
		LightSpaceBox.MinPt.Y, LightSpaceBox.MaxPt.Y,
		LightSpaceBox.MinPt.Z, LightSpaceBox.MaxPt.Z
	);

	// 카메라 뷰와 결합
	OutView = CameraView * OutView;

	Params.PPNear = LightSpaceBox.MinPt.Z;
	Params.PPFar = LightSpaceBox.MaxPt.Z;
}

//-----------------------------------------------------------------------------
// PSM (원근 그림자 맵)
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

	// 단계 1: 슬라이드 백이 적용된 가상 카메라 설정
	FMatrix VirtualCameraView = FMatrix::Identity();
	FMatrix VirtualCameraProj;

	float ZNear = Params.ZNear;
	float ZFar = Params.ZFar;
	float SlideBack = 0.0f;

	if (Params.bSlideBackEnabled)
	{
		// 무한 평면 거리 계산
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
			// 경계 원뿔을 사용하여 타이트한 FOV 계산
			FVector EyePos = FVector::ZeroVector();
			FVector EyeDir = FVector(0, 0, 1);  // 뷰 공간의 Z-Forward (DirectX 표준)
			FPSMBoundingCone BC(ShadowReceivers, VirtualCameraView, EyePos, EyeDir);

			float Width = 2.0f * std::tan(BC.FovX) * ZNear;
			float Height = 2.0f * std::tan(BC.FovY) * ZNear;
			VirtualCameraProj = FMatrix::CreatePerspectiveLH(Width, Height, ZNear, ZFar);
		}
		else
		{
			// 슬라이드 백 조정이 적용된 카메라 FOV 사용
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

	// 단계 2: 빛 방향을 포스트-프로젝티브 공간으로 변환
	FVector4 LightDirW(ViewLightDir.X, ViewLightDir.Y, ViewLightDir.Z, 0.0f);
	FVector4 PPLight = VirtualCameraProj.TransformVector4(LightDirW);

	Params.bShadowTestInverted = (PPLight.W < 0.0f);

	// 단계 3: 투영 타입 결정
	if (std::abs(PPLight.W) <= W_EPSILON)
	{
		// 빛이 무한대에 있음 - 직교 투영 사용
		BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera,
			ShadowCasters, ShadowReceivers, Params);
		return;
	}

	// 단계 4: 원근 PSM
	FVector PPLightPos(
		PPLight.X / PPLight.W,
		PPLight.Y / PPLight.W,
		PPLight.Z / PPLight.W
	);

	FMatrix LightView, LightProj;

	const FVector PPCubeCenter(0.0f, 0.0f, 0.5f);

	if (Params.bShadowTestInverted)
	{
		// 역 투영 (빛이 카메라 뒤에 있음)
		FPSMBoundingCone ViewCone;
		if (!Params.bUnitCubeClip)
		{
			// 전체 유닛 큐브 투영
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
		// 일반 투영
		FVector LookAt = PPCubeCenter - PPLightPos;
		float Distance = LookAt.Length();
		LookAt = LookAt * (1.0f / Distance);

		FVector Up = ZUp;
		if (std::abs(ZUp.Dot(LookAt)) > 0.99f)
			Up = XForward;

		if (!Params.bUnitCubeClip)
		{
			// 간단한 구 기반 FOV
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
			// 유닛 큐브 클리핑
			FPSMBoundingCone BC(ShadowReceivers, EyeToPostProjective, PPLightPos);

			LightView = BC.LookAtMatrix;

			float Fovy = 2.0f * BC.FovY;
			float Aspect = BC.FovX / BC.FovY;
			float FNear = BC.Near * 0.6f;  // 약간의 조정
			float FFar = BC.Far;

			FNear = std::max(0.001f, FNear);

			Params.PPNear = FNear;
			Params.PPFar = FFar;

			LightProj = FMatrix::CreatePerspectiveFovLH(Fovy, Aspect, FNear, FFar);
		}
	}

	// 단계 5: 행렬 결합
	FMatrix LightViewProj = LightView * LightProj;
	OutView = VirtualCameraViewProj;
	OutProj = LightViewProj;

	// 참고: 실제 렌더링에서는 다음과 같이 결합됨: FinalMatrix = View * VirtualView * VirtualProj * LightView * LightProj
	// 하지만 ShadowMapPass를 위해 최종 결합 행렬을 ViewProj로 반환
	OutView = CameraView;
	OutProj = VirtualCameraView * VirtualCameraProj * LightView * LightProj;

	Params.SlideBack = SlideBack;
}

//-----------------------------------------------------------------------------
// LSPSM & TSM - 플레이스홀더 구현
// 이들은 복잡하며, 필요시 나중에 추가될 수 있습니다
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
	// 현재는 uniform으로 대체
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
	// 현재는 uniform으로 대체
	BuildUniformShadowMap(OutView, OutProj, LightDirection, Camera, ShadowCasters, ShadowReceivers, Params);
}
