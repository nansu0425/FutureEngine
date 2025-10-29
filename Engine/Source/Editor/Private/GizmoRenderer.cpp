#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/GizmoGeometry.h"
#include "Editor/Public/GizmoMath.h"

#include "Editor/Public/Editor.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/Renderer/Public/Renderer.h"

void FGizmoBatchRenderer::AddMesh(const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices,
                                   const FVector4& InColor, const FVector& InLocation,
                                   const FQuaternion& InRotation, const FVector& InScale)
{
	FBatchedMesh Mesh;
	Mesh.Vertices = InVertices;
	Mesh.Indices = InIndices;
	Mesh.Color = InColor;
	Mesh.Location = InLocation;
	Mesh.Rotation = InRotation;
	Mesh.Scale = InScale;
	Meshes.push_back(Mesh);
}

void FGizmoBatchRenderer::FlushAndRender(const FRenderState& InRenderState)
{
	if (Meshes.empty())
	{
		return;
	}

	URenderer& Renderer = URenderer::GetInstance();

	for (const FBatchedMesh& Mesh : Meshes)
	{
		if (Mesh.Vertices.empty() || Mesh.Indices.empty())
		{
			continue;
		}

		// 임시 버퍼 생성
		ID3D11Buffer* TempVB = nullptr;
		ID3D11Buffer* TempIB = nullptr;

		D3D11_BUFFER_DESC VBDesc = {};
		VBDesc.Usage = D3D11_USAGE_DEFAULT;
		VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * Mesh.Vertices.size());
		VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA VBData = {};
		VBData.pSysMem = Mesh.Vertices.data();
		Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

		D3D11_BUFFER_DESC IBDesc = {};
		IBDesc.Usage = D3D11_USAGE_DEFAULT;
		IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Mesh.Indices.size());
		IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA IBData = {};
		IBData.pSysMem = Mesh.Indices.data();
		Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

		// Primitive 설정 및 렌더링
		FEditorPrimitive PrimitiveInfo;
		PrimitiveInfo.Location = Mesh.Location;
		PrimitiveInfo.Rotation = Mesh.Rotation;
		PrimitiveInfo.Scale = Mesh.Scale;
		PrimitiveInfo.VertexBuffer = TempVB;
		PrimitiveInfo.NumVertices = static_cast<uint32>(Mesh.Vertices.size());
		PrimitiveInfo.IndexBuffer = TempIB;
		PrimitiveInfo.NumIndices = static_cast<uint32>(Mesh.Indices.size());
		PrimitiveInfo.Color = Mesh.Color;
		PrimitiveInfo.bShouldAlwaysVisible = Mesh.bAlwaysVisible;
		PrimitiveInfo.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		Renderer.RenderEditorPrimitive(PrimitiveInfo, InRenderState);

		// 버퍼 해제
		TempVB->Release();
		TempIB->Release();
	}

	Clear();
}

void FGizmoBatchRenderer::Clear()
{
	Meshes.clear();
}

void UGizmo::RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	TargetComponent = Cast<USceneComponent>(GEditor->GetEditorModule()->GetSelectedComponent());
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// 스크린에서 균일한 사이즈를 가지도록 하기 위한 스케일 조정
	const FVector GizmoLocation = TargetComponent->GetWorldLocation();
	const float RenderScale = FGizmoMath::CalculateScreenSpaceScale(InCamera, InViewport, GizmoLocation, 120.0f);

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

	// 회전 모드 확인
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	bool bIsRotateMode = (GizmoMode == EGizmoMode::Rotate);

	// 각 링을 해당 평면으로 회전시키는 변환 (드래그 시 Ring 사용)
	FQuaternion AxisRots[3] = {
		FQuaternion::Identity(),  // X링: YZ 평면
		FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(-90.0f)),  // Y링: XZ 평면
		FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(90.0f))  // Z링: XY 평면 - Right(Y)축 기준 90도로 X→Z
	};

	// 오쏘 뷰에서 World 모드일 때 어떤 축을 표시할지 결정
	const bool bIsOrtho = (InCamera->GetCameraType() == ECameraType::ECT_Orthographic);
	EGizmoDirection OrthoWorldAxis = EGizmoDirection::None;

	if (bIsOrtho && bIsWorld && bIsRotateMode)
	{
		// 카메라 Forward 방향으로 어떤 평면을 보고 있는지 판단
		const FVector CamForward = InCamera->GetForward();
		const float AbsX = std::abs(CamForward.X);
		const float AbsY = std::abs(CamForward.Y);
		const float AbsZ = std::abs(CamForward.Z);

		// 가장 지배적인 축 방향 찾기
		if (AbsZ > AbsX && AbsZ > AbsY)
		{
			// Top/Bottom 뷰 (XY 평면) -> Z축 회전
			OrthoWorldAxis = EGizmoDirection::Up;
		}
		else if (AbsY > AbsX && AbsY > AbsZ)
		{
			// Left/Right 뷰 (XZ 평면) -> Y축 회전
			OrthoWorldAxis = EGizmoDirection::Right;
		}
		else
		{
			// Front/Back 뷰 (YZ 평면) -> X축 회전
			OrthoWorldAxis = EGizmoDirection::Forward;
		}
	}

	// Rotate 모드에서 드래그 중이면 해당 축만 표시
	// 오쏘 뷰 + World 모드면 단일 축만 표시
	bool bShowX = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Forward;
	bool bShowY = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Right;
	bool bShowZ = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Up;

	if (bIsOrtho && bIsWorld && bIsRotateMode && !bIsDragging)
	{
		bShowX = (OrthoWorldAxis == EGizmoDirection::Forward);
		bShowY = (OrthoWorldAxis == EGizmoDirection::Right);
		bShowZ = (OrthoWorldAxis == EGizmoDirection::Up);
	}

	// 평면 기즈모가 하이라이팅되지 않은 경우 먼저 렌더링 (축이 위에 그려지도록)
	bool bAnyPlaneHighlighted = (GizmoDirection == EGizmoDirection::XY_Plane ||
	                              GizmoDirection == EGizmoDirection::XZ_Plane ||
	                              GizmoDirection == EGizmoDirection::YZ_Plane);

	if (!bAnyPlaneHighlighted)
	{
		if (GizmoMode == EGizmoMode::Translate)
		{
			RenderTranslatePlanes(P, BaseRot, RenderScale);
		}
		else if (GizmoMode == EGizmoMode::Scale)
		{
			RenderScalePlanes(P, BaseRot, RenderScale);
		}
	}

	// X축 (Forward) - 빨간색
	if (bShowX)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Forward)
		{
			RenderRotationCircles(P, AxisRots[0], BaseRot, GizmoColor[0]);
		}
		else if (bIsRotateMode)
		{
			RenderRotationQuarterRing(P, BaseRot, 0, EGizmoDirection::Forward, InCamera);
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = AxisRots[0] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Forward);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}

	// Y축 (Right) - 초록색
	if (bShowY)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Right)
		{
			RenderRotationCircles(P, AxisRots[1], BaseRot, GizmoColor[1]);
		}
		else if (bIsRotateMode)
		{
			RenderRotationQuarterRing(P, BaseRot, 1, EGizmoDirection::Right, InCamera);
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = AxisRots[1] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Right);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}

	// Z축 (Up) - 파란색
	if (bShowZ)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Up)
		{
			RenderRotationCircles(P, AxisRots[2], BaseRot, GizmoColor[2]);
		}
		else if (bIsRotateMode)
		{
			RenderRotationQuarterRing(P, BaseRot, 2, EGizmoDirection::Up, InCamera);
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = AxisRots[2] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Up);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}

	// 중심 구체 렌더링
	if (GizmoMode == EGizmoMode::Translate || GizmoMode == EGizmoMode::Scale)
	{
		RenderCenterSphere(P, RenderScale);
	}

	// 평면 기즈모가 하이라이팅된 경우 마지막에 렌더링 (축 위에 보이도록)
	if (bAnyPlaneHighlighted)
	{
		if (GizmoMode == EGizmoMode::Translate)
		{
			RenderTranslatePlanes(P, BaseRot, RenderScale);
		}
		else if (GizmoMode == EGizmoMode::Scale)
		{
			RenderScalePlanes(P, BaseRot, RenderScale);
		}
	}
}

/**
 * @brief Translation 및 Scale 모드의 중심 구체 렌더링
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderCenterSphere(const FEditorPrimitive& P, float RenderScale)
{
	const float SphereRadius = 0.10f * RenderScale;
	constexpr int NumSegments = 16;
	constexpr int NumRings = 8;

	TArray<FNormalVertex> vertices;
	TArray<uint32> Indices;

	// UV Sphere 생성
	for (int ring = 0; ring <= NumRings; ++ring)
	{
		float phi = static_cast<float>(ring) / NumRings * PI;
		float sinPhi = std::sin(phi);
		float cosPhi = std::cos(phi);

		for (int seg = 0; seg <= NumSegments; ++seg)
		{
			float theta = static_cast<float>(seg) / NumSegments * 2.0f * PI;
			float sinTheta = std::sin(theta);
			float cosTheta = std::cos(theta);

			FVector pos(sinPhi * cosTheta, sinPhi * sinTheta, cosPhi);
			FVector normal = pos.GetNormalized();
			pos = pos * SphereRadius;

			vertices.push_back({pos, normal});
		}
	}

	// 인덱스 생성
	for (int ring = 0; ring < NumRings; ++ring)
	{
		for (int seg = 0; seg < NumSegments; ++seg)
		{
			int current = ring * (NumSegments + 1) + seg;
			int next = current + NumSegments + 1;

			Indices.push_back(current);
			Indices.push_back(next);
			Indices.push_back(current + 1);

			Indices.push_back(current + 1);
			Indices.push_back(next);
			Indices.push_back(next + 1);
		}
	}

	// 중심 구체 색상: 흰색 또는 하이라이트
	bool bIsCenterSelected = (GizmoDirection == EGizmoDirection::Center);
	FVector4 SphereColor;
	if (bIsCenterSelected && bIsDragging)
	{
		SphereColor = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
	}
	else if (bIsCenterSelected)
	{
		SphereColor = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
	}
	else
	{
		SphereColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 기본: 흰색
	}

	// 배칭에 추가 및 즉시 렌더링
	FGizmoBatchRenderer Batch;
	Batch.AddMesh(vertices, Indices, SphereColor, P.Location);
	Batch.FlushAndRender(RenderState);
}

/**
 * @brief Translation 모드의 평면 기즈모 렌더링 (직각 표시)
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param BaseRot 기즈모 회전 (World/Local 모드)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderTranslatePlanes(const FEditorPrimitive& P, const FQuaternion& BaseRot, float RenderScale)
{
	const float CornerPos = 0.3f * RenderScale;
	const float HandleRadius = 0.02f * RenderScale;
	const int NumSegments = 8;

	struct FPlaneInfo
	{
		EGizmoDirection Direction;
		FVector Tangent1;
		FVector Tangent2;
	};

	FPlaneInfo Planes[3] = {
		{EGizmoDirection::XY_Plane, {1, 0, 0}, {0, 1, 0}},
		{EGizmoDirection::XZ_Plane, {1, 0, 0}, {0, 0, 1}},
		{EGizmoDirection::YZ_Plane, {0, 1, 0}, {0, 0, 1}}
	};

	FGizmoBatchRenderer Batch;

	for (const FPlaneInfo& PlaneInfo : Planes)
	{
		FVector T1 = PlaneInfo.Tangent1;
		FVector T2 = PlaneInfo.Tangent2;

		if (!bIsWorld)
		{
			T1 = BaseRot.RotateVector(T1);
			T2 = BaseRot.RotateVector(T2);
		}

		FVector PlaneNormal = Cross(T1, T2).GetNormalized();

		EGizmoDirection Seg1Color, Seg2Color;
		if (PlaneInfo.Direction == EGizmoDirection::XY_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Right;
		}
		else if (PlaneInfo.Direction == EGizmoDirection::XZ_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Up;
		}
		else
		{
			Seg1Color = EGizmoDirection::Right;
			Seg2Color = EGizmoDirection::Up;
		}

		bool bIsPlaneSelected = (GizmoDirection == PlaneInfo.Direction);

		// 색상 계산
		FVector4 Seg1Color_Final, Seg2Color_Final;
		if (bIsPlaneSelected && bIsDragging)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
		}
		else if (bIsPlaneSelected)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			Seg1Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg1Color)];
			Seg2Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg2Color)];
		}

		// 선분 1 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start1 = T1 * CornerPos;
			FVector End1 = T1 * CornerPos + T2 * CornerPos;
			FVector Dir1 = (End1 - Start1).GetNormalized();
			FVector Perp1_1 = Cross(Dir1, PlaneNormal).GetNormalized();
			FVector Perp1_2 = Cross(Dir1, Perp1_1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1_1 * std::cos(Angle) + Perp1_2 * std::sin(Angle)) * HandleRadius;

				vertices.push_back({Start1 + Offset, Offset.GetNormalized()});
				vertices.push_back({End1 + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.push_back(i * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg1Color_Final, P.Location);
		}

		// 선분 2 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start2 = T2 * CornerPos;
			FVector End2 = T1 * CornerPos + T2 * CornerPos;
			FVector Dir2 = (End2 - Start2).GetNormalized();
			FVector Perp2_1 = Cross(Dir2, PlaneNormal).GetNormalized();
			FVector Perp2_2 = Cross(Dir2, Perp2_1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp2_1 * std::cos(Angle) + Perp2_2 * std::sin(Angle)) * HandleRadius;

				vertices.push_back({Start2 + Offset, Offset.GetNormalized()});
				vertices.push_back({End2 + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.push_back(i * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg2Color_Final, P.Location);
		}
	}

	// 모든 메쉬를 일괄 렌더링
	Batch.FlushAndRender(RenderState);
}

/**
 * @brief Scale 모드의 평면 기즈모 렌더링 (대각선 형태)
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param BaseRot 기즈모 회전 (항상 로컬 좌표)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderScalePlanes(const FEditorPrimitive& P, const FQuaternion& BaseRot, float RenderScale)
{
	const float MidPoint = 0.5f * RenderScale;
	const float HandleRadius = 0.02f * RenderScale;
	constexpr int NumSegments = 8;

	struct FPlaneInfo
	{
		EGizmoDirection Direction;
		FVector Tangent1;
		FVector Tangent2;
	};

	FPlaneInfo Planes[3] = {
		{EGizmoDirection::XY_Plane, {1, 0, 0}, {0, 1, 0}},
		{EGizmoDirection::XZ_Plane, {1, 0, 0}, {0, 0, 1}},
		{EGizmoDirection::YZ_Plane, {0, 1, 0}, {0, 0, 1}}
	};

	FGizmoBatchRenderer Batch;

	for (const FPlaneInfo& PlaneInfo : Planes)
	{
		FVector T1 = BaseRot.RotateVector(PlaneInfo.Tangent1);
		FVector T2 = BaseRot.RotateVector(PlaneInfo.Tangent2);

		FVector Point1 = T1 * MidPoint;
		FVector Point2 = T2 * MidPoint;
		FVector MidCenter = (Point1 + Point2) * 0.5f;

		FVector PlaneNormal = Cross(T1, T2).GetNormalized();

		EGizmoDirection Seg1Color, Seg2Color;
		if (PlaneInfo.Direction == EGizmoDirection::XY_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Right;
		}
		else if (PlaneInfo.Direction == EGizmoDirection::XZ_Plane)
		{
			Seg1Color = EGizmoDirection::Forward;
			Seg2Color = EGizmoDirection::Up;
		}
		else
		{
			Seg1Color = EGizmoDirection::Right;
			Seg2Color = EGizmoDirection::Up;
		}

		bool bIsPlaneSelected = (GizmoDirection == PlaneInfo.Direction);
		bool bIsCenterSelected = (GizmoDirection == EGizmoDirection::Center);

		// 색상 계산 (평면 자체 선택 또는 Center 선택 시 하이라이팅)
		FVector4 Seg1Color_Final, Seg2Color_Final;
		if ((bIsPlaneSelected || bIsCenterSelected) && bIsDragging)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
		}
		else if (bIsPlaneSelected || bIsCenterSelected)
		{
			Seg1Color_Final = Seg2Color_Final = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else
		{
			Seg1Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg1Color)];
			Seg2Color_Final = GizmoColor[FGizmoMath::DirectionToAxisIndex(Seg2Color)];
		}

		// 선분 1 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start = Point1;
			FVector End = MidCenter;
			FVector DiagDir = (End - Start).GetNormalized();
			FVector Perp1 = Cross(DiagDir, PlaneNormal).GetNormalized();
			FVector Perp2 = Cross(DiagDir, Perp1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1 * std::cos(Angle) + Perp2 * std::sin(Angle)) * HandleRadius;

				vertices.push_back({Start + Offset, Offset.GetNormalized()});
				vertices.push_back({End + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.push_back(i * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg1Color_Final, P.Location);
		}

		// 선분 2 메쉬 생성
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> Indices;

			FVector Start = MidCenter;
			FVector End = Point2;
			FVector DiagDir = (End - Start).GetNormalized();
			FVector Perp1 = Cross(DiagDir, PlaneNormal).GetNormalized();
			FVector Perp2 = Cross(DiagDir, Perp1).GetNormalized();

			for (int i = 0; i < NumSegments; ++i)
			{
				float Angle = static_cast<float>(i) / NumSegments * 2.0f * PI;
				FVector Offset = (Perp1 * std::cos(Angle) + Perp2 * std::sin(Angle)) * HandleRadius;

				vertices.push_back({Start + Offset, Offset.GetNormalized()});
				vertices.push_back({End + Offset, Offset.GetNormalized()});
			}

			for (int i = 0; i < NumSegments; ++i)
			{
				int Next = (i + 1) % NumSegments;
				Indices.push_back(i * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(Next * 2 + 0);
				Indices.push_back(i * 2 + 1);
				Indices.push_back(Next * 2 + 1);
			}

			Batch.AddMesh(vertices, Indices, Seg2Color_Final, P.Location);
		}
	}

	// 모든 메쉬를 일괄 렌더링
	Batch.FlushAndRender(RenderState);
}

void UGizmo::RenderRotationCircles(const FEditorPrimitive& P, const FQuaternion& AxisRotation,
	const FQuaternion& BaseRot, const FVector4& AxisColor)
{
	URenderer& Renderer = URenderer::GetInstance();

	// Inner circle: 두꺼운 축 색상 선
	TArray<FNormalVertex> innerVertices, outerVertices;
	TArray<uint32> innerIndices, outerIndices;

	const float InnerLineThickness = RotateCollisionConfig.Thickness * 2.0f;
	FGizmoGeometry::GenerateCircleLineMesh(FVector(0, 0, 1), FVector(0, 1, 0),
		RotateCollisionConfig.InnerRadius, InnerLineThickness, innerVertices, innerIndices);

	if (!innerIndices.empty())
	{
		ID3D11Buffer* innerVB = nullptr;
		ID3D11Buffer* innerIB = nullptr;
		FGizmoGeometry::CreateTempBuffers(innerVertices, innerIndices, &innerVB, &innerIB);

		FEditorPrimitive InnerPrim = P;
		InnerPrim.VertexBuffer = innerVB;
		InnerPrim.NumVertices = static_cast<uint32>(innerVertices.size());
		InnerPrim.IndexBuffer = innerIB;
		InnerPrim.NumIndices = static_cast<uint32>(innerIndices.size());
		InnerPrim.Rotation = AxisRotation * BaseRot;
		InnerPrim.Color = AxisColor;
		Renderer.RenderEditorPrimitive(InnerPrim, RenderState);

		innerVB->Release();
		innerIB->Release();
	}

	// Outer circle: 얇은 노란색 선
	const float OuterLineThickness = RotateCollisionConfig.Thickness * 1.0f;
	FGizmoGeometry::GenerateCircleLineMesh(FVector(0, 0, 1), FVector(0, 1, 0),
		RotateCollisionConfig.OuterRadius, OuterLineThickness, outerVertices, outerIndices);

	if (!outerIndices.empty())
	{
		ID3D11Buffer* outerVB = nullptr;
		ID3D11Buffer* outerIB = nullptr;
		FGizmoGeometry::CreateTempBuffers(outerVertices, outerIndices, &outerVB, &outerIB);

		FEditorPrimitive OuterPrim = P;
		OuterPrim.VertexBuffer = outerVB;
		OuterPrim.NumVertices = static_cast<uint32>(outerVertices.size());
		OuterPrim.IndexBuffer = outerIB;
		OuterPrim.NumIndices = static_cast<uint32>(outerIndices.size());
		OuterPrim.Rotation = AxisRotation * BaseRot;
		OuterPrim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
		Renderer.RenderEditorPrimitive(OuterPrim, RenderState);

		outerVB->Release();
		outerIB->Release();
	}

	// 각도 눈금 렌더링 (스냅 각도마다 작은 눈금, 90도마다 큰 눈금)
	{
		TArray<FNormalVertex> tickVertices;
		TArray<uint32> tickIndices;

		const float SnapAngle = UViewportManager::GetInstance().GetRotationSnapAngle();
		FGizmoGeometry::GenerateAngleTickMarks(FVector(0, 0, 1), FVector(0, 1, 0),
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
			RotateCollisionConfig.Thickness, SnapAngle, tickVertices, tickIndices);

		if (!tickIndices.empty())
		{
			ID3D11Buffer* tickVB = nullptr;
			ID3D11Buffer* tickIB = nullptr;
			FGizmoGeometry::CreateTempBuffers(tickVertices, tickIndices, &tickVB, &tickIB);

			FEditorPrimitive TickPrim = P;
			TickPrim.VertexBuffer = tickVB;
			TickPrim.NumVertices = static_cast<uint32>(tickVertices.size());
			TickPrim.IndexBuffer = tickIB;
			TickPrim.NumIndices = static_cast<uint32>(tickIndices.size());
			TickPrim.Rotation = AxisRotation * BaseRot;
			TickPrim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			Renderer.RenderEditorPrimitive(TickPrim, RenderState);

			tickVB->Release();
			tickIB->Release();
		}
	}

	// 회전 각도 Arc 렌더링 (스냅이 켜져있으면 스냅된 각도, 아니면 현재 각도)
	float DisplayAngle = CurrentRotationAngle;
	if (UViewportManager::GetInstance().IsRotationSnapEnabled())
	{
		const float SnapAngle = UViewportManager::GetInstance().GetRotationSnapAngle();
		DisplayAngle = GetSnappedRotationAngle(SnapAngle);
	}
	if (std::abs(DisplayAngle) > 0.001f)
	{
		TArray<FNormalVertex> arcVertices;
		TArray<uint32> arcIndices;

		// Arc를 약간 더 두껍게 (링보다 살짝 크게)
		const float ArcInnerRadius = RotateCollisionConfig.InnerRadius * 0.98f;
		const float ArcOuterRadius = RotateCollisionConfig.OuterRadius * 1.02f;

		FGizmoGeometry::GenerateRotationArcMesh(FVector(0, 0, 1), FVector(0, 1, 0),
			ArcInnerRadius, ArcOuterRadius, RotateCollisionConfig.Thickness,
			DisplayAngle, FVector(0,0,0), arcVertices, arcIndices);

		if (!arcIndices.empty())
		{
			ID3D11Buffer* arcVB = nullptr;
			ID3D11Buffer* arcIB = nullptr;
			FGizmoGeometry::CreateTempBuffers(arcVertices, arcIndices, &arcVB, &arcIB);

			FEditorPrimitive ArcPrim = P;
			ArcPrim.VertexBuffer = arcVB;
			ArcPrim.NumVertices = static_cast<uint32>(arcVertices.size());
			ArcPrim.IndexBuffer = arcIB;
			ArcPrim.NumIndices = static_cast<uint32>(arcIndices.size());
			ArcPrim.Rotation = AxisRotation * BaseRot;
			ArcPrim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f); // 노란색
			Renderer.RenderEditorPrimitive(ArcPrim, RenderState);

			arcVB->Release();
			arcIB->Release();
		}
	}
}

void UGizmo::RenderRotationQuarterRing(const FEditorPrimitive& P, const FQuaternion& BaseRot,
	int32 AxisIndex, EGizmoDirection Direction, UCamera* InCamera)
{
	URenderer& Renderer = URenderer::GetInstance();
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	const bool bIsOrtho = InCamera->GetCameraType() == ECameraType::ECT_Orthographic;

	// Gizmo axis definitions
	const FVector LocalAxis0[3] = {
		FVector(0.0f, 0.0f, 1.0f),  // X ring: Z axis
		FVector(1.0f, 0.0f, 0.0f),  // Y ring: X axis
		FVector(1.0f, 0.0f, 0.0f)   // Z ring: X axis
	};
	const FVector LocalAxis1[3] = {
		FVector(0.0f, 1.0f, 0.0f),  // X ring: Y axis
		FVector(0.0f, 0.0f, 1.0f),  // Y ring: Z axis
		FVector(0.0f, 1.0f, 0.0f)   // Z ring: Y axis
	};

	// 각 링을 해당 평면으로 회전시키는 변환
	FQuaternion AxisRots[3] = {
		FQuaternion::Identity(),  // X링: YZ 평면
		FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(-90.0f)),  // Y링: XZ 평면
		FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(90.0f))  // Z링: XY 평면
	};

	if (bIsOrtho && bIsWorld)
	{
		// 오쏘 뷰 + World 모드: Full Ring
		FEditorPrimitive RingPrim = P;
		RingPrim.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
		RingPrim.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
		RingPrim.IndexBuffer = nullptr;
		RingPrim.NumIndices = 0;
		RingPrim.Rotation = AxisRots[AxisIndex] * BaseRot;
		RingPrim.Color = ColorFor(Direction);
		Renderer.RenderEditorPrimitive(RingPrim, RenderState);
	}
	else
	{
		// 퍼스펙티브 또는 오쏘 뷰 Local 모드: QuarterRing
		const FVector GizmoLoc = P.Location;
		const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
		const FVector CameraLoc = InCamera->GetLocation();
		const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

		FVector Axis0 = bIsWorld ? FGizmoConstants::LocalAxis0[AxisIndex] : GizmoRot.RotateVector(FGizmoConstants::LocalAxis0[AxisIndex]);
		FVector Axis1 = bIsWorld ? FGizmoConstants::LocalAxis1[AxisIndex] : GizmoRot.RotateVector(FGizmoConstants::LocalAxis1[AxisIndex]);

		const bool bMirrorAxis0 = (Axis0.Dot(DirectionToWidget) <= 0.0f);
		const bool bMirrorAxis1 = (Axis1.Dot(DirectionToWidget) <= 0.0f);
		const FVector RenderAxis0 = bMirrorAxis0 ? Axis0 : -Axis0;
		const FVector RenderAxis1 = bMirrorAxis1 ? Axis1 : -Axis1;

		FVector LocalRenderAxis0 = bIsWorld ? RenderAxis0 : GizmoRot.Inverse().RotateVector(RenderAxis0);
		FVector LocalRenderAxis1 = bIsWorld ? RenderAxis1 : GizmoRot.Inverse().RotateVector(RenderAxis1);
		LocalRenderAxis0.Normalize();
		LocalRenderAxis1.Normalize();

		TArray<FNormalVertex> vertices;
		TArray<uint32> Indices;
		FGizmoGeometry::GenerateQuarterRingMesh(LocalRenderAxis0, LocalRenderAxis1,
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius, RotateCollisionConfig.Thickness,
			vertices, Indices);

		ID3D11Buffer* TempVB = nullptr;
		ID3D11Buffer* TempIB = nullptr;
		FGizmoGeometry::CreateTempBuffers(vertices, Indices, &TempVB, &TempIB);

		FEditorPrimitive QuarterPrim = P;
		QuarterPrim.VertexBuffer = TempVB;
		QuarterPrim.NumVertices = static_cast<uint32>(vertices.size());
		QuarterPrim.IndexBuffer = TempIB;
		QuarterPrim.NumIndices = static_cast<uint32>(Indices.size());
		QuarterPrim.Rotation = BaseRot;
		QuarterPrim.Color = ColorFor(Direction);
		Renderer.RenderEditorPrimitive(QuarterPrim, RenderState);

		TempVB->Release();
		TempIB->Release();
	}
}

void UGizmo::RenderForHitProxy(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (!TargetComponent || !InCamera)
	{
		return;
	}

	// Scale uniformly in screen space
	const FVector GizmoLocation = TargetComponent->GetWorldLocation();
	const float RenderScale = FGizmoMath::CalculateScreenSpaceScale(InCamera, InViewport, GizmoLocation, 120.0f);

	URenderer& Renderer = URenderer::GetInstance();
	FHitProxyManager& HitProxyManager = FHitProxyManager::GetInstance();

	const int Mode = static_cast<int>(GizmoMode);
	auto& P = Primitives[Mode];
	P.Location = TargetComponent->GetWorldLocation();
	P.Scale = FVector(RenderScale, RenderScale, RenderScale);

	// Determine gizmo base rotation
	FQuaternion BaseRot;
	if (GizmoMode == EGizmoMode::Rotate && !bIsWorld && bIsDragging)
	{
		BaseRot = DragStartActorRotationQuat;
	}
	else if (GizmoMode == EGizmoMode::Scale)
	{
		BaseRot = TargetComponent->GetWorldRotationAsQuaternion();
	}
	else
	{
		BaseRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	}

	bool bIsRotateMode = (GizmoMode == EGizmoMode::Rotate);

	FQuaternion AxisRots[3];
	if (bIsRotateMode)
	{
		// Rotation mode
		AxisRots[0] = FQuaternion::Identity();
		AxisRots[1] = FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(-90.0f));
		AxisRots[2] = FQuaternion::FromAxisAngle(FVector::ForwardVector(), FVector::GetDegreeToRadian(90.0f));
	}
	else
	{
		// Translation/Scale mode
		AxisRots[0] = FQuaternion::Identity();
		AxisRots[1] = FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(-90.0f));
		AxisRots[2] = FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(90.0f));
	}
	const bool bIsOrtho = (InCamera->GetCameraType() == ECameraType::ECT_Orthographic);

	// Orthographic rotation axis determination
	EGizmoDirection OrthoWorldAxis = EGizmoDirection::None;
	if (bIsOrtho && bIsWorld && bIsRotateMode)
	{
		const FVector CamForward = InCamera->GetForward();
		const float AbsX = abs(CamForward.X);
		const float AbsY = abs(CamForward.Y);
		const float AbsZ = abs(CamForward.Z);

		if (AbsZ > AbsX && AbsZ > AbsY)
		{
			OrthoWorldAxis = EGizmoDirection::Up;
		}
		else if (AbsY > AbsX && AbsY > AbsZ)
		{
			OrthoWorldAxis = EGizmoDirection::Right;
		}
		else
		{
			OrthoWorldAxis = EGizmoDirection::Forward;
		}
	}

	bool bShowX = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Forward;
	bool bShowY = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Right;
	bool bShowZ = !bIsDragging || !bIsRotateMode || GizmoDirection == EGizmoDirection::Up;

	if (bIsOrtho && bIsWorld && bIsRotateMode && !bIsDragging)
	{
		bShowX = (OrthoWorldAxis == EGizmoDirection::Forward);
		bShowY = (OrthoWorldAxis == EGizmoDirection::Right);
		bShowZ = (OrthoWorldAxis == EGizmoDirection::Up);
	}

	FRenderState RenderState;
	RenderState.FillMode = EFillMode::Solid;
	RenderState.CullMode = ECullMode::Back;

	// Allocate HitProxy IDs per axis
	HWidgetAxis* XAxisProxy = new HWidgetAxis(EGizmoAxisType::X, InvalidHitProxyId);
	HWidgetAxis* YAxisProxy = new HWidgetAxis(EGizmoAxisType::Y, InvalidHitProxyId);
	HWidgetAxis* ZAxisProxy = new HWidgetAxis(EGizmoAxisType::Z, InvalidHitProxyId);
	HWidgetAxis* CenterProxy = new HWidgetAxis(EGizmoAxisType::Center, InvalidHitProxyId);

	FHitProxyId XAxisId = HitProxyManager.AllocateHitProxyId(XAxisProxy);
	FHitProxyId YAxisId = HitProxyManager.AllocateHitProxyId(YAxisProxy);
	FHitProxyId ZAxisId = HitProxyManager.AllocateHitProxyId(ZAxisProxy);
	FHitProxyId CenterId = HitProxyManager.AllocateHitProxyId(CenterProxy);


	// Rotation mode requires dynamic mesh generation
	if (bIsRotateMode)
	{
		// NOTE: ID 스왑은 의도된 설계, LocalAxis 배열이 회전 평면 기준이므로 bShowX는 YZ평면(Y축 회전)을 생성
		// 따라서 물리적 위치와 ID를 매칭하려면 bShowX -> YAxisId, bShowY -> XAxisId 할당 필요

		// X axis rendering
		if (bShowX)
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;
			FGizmoGeometry::GenerateQuarterRingMesh(FVector(0, 0, 1), FVector(0, 1, 0),
				RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
				RotateCollisionConfig.Thickness, vertices, indices);

			if (!indices.empty())
			{
				ID3D11Buffer* vb = nullptr;
				ID3D11Buffer* ib = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &vb, &ib);

				P.VertexBuffer = vb;
				P.NumVertices = static_cast<uint32>(vertices.size());
				P.IndexBuffer = ib;
				P.NumIndices = static_cast<uint32>(indices.size());
				P.Rotation = AxisRots[0] * BaseRot;
				P.Color = YAxisId.GetColor();
				Renderer.RenderEditorPrimitive(P, RenderState);

				vb->Release();
				ib->Release();
			}
		}

		// Y axis rendering
		if (bShowY)
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;
			FGizmoGeometry::GenerateQuarterRingMesh(FVector(1, 0, 0), FVector(0, 0, 1),
				RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
				RotateCollisionConfig.Thickness, vertices, indices);

			if (!indices.empty())
			{
				ID3D11Buffer* vb = nullptr;
				ID3D11Buffer* ib = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &vb, &ib);

				P.VertexBuffer = vb;
				P.NumVertices = static_cast<uint32>(vertices.size());
				P.IndexBuffer = ib;
				P.NumIndices = static_cast<uint32>(indices.size());
				P.Rotation = AxisRots[1] * BaseRot;
				P.Color = XAxisId.GetColor();
				Renderer.RenderEditorPrimitive(P, RenderState);

				vb->Release();
				ib->Release();
			}
		}

		// Z axis rendering
		if (bShowZ)
		{
			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;
			FGizmoGeometry::GenerateQuarterRingMesh(FVector(1, 0, 0), FVector(0, 1, 0),
				RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
				RotateCollisionConfig.Thickness, vertices, indices);

			if (!indices.empty())
			{
				ID3D11Buffer* vb = nullptr;
				ID3D11Buffer* ib = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &vb, &ib);

				P.VertexBuffer = vb;
				P.NumVertices = static_cast<uint32>(vertices.size());
				P.IndexBuffer = ib;
				P.NumIndices = static_cast<uint32>(indices.size());
				P.Rotation = AxisRots[2] * BaseRot;
				P.Color = ZAxisId.GetColor();
				Renderer.RenderEditorPrimitive(P, RenderState);

				vb->Release();
				ib->Release();
			}
		}
	}
	else
	{
		// Translate/Scale mode uses existing Primitives
		// X axis rendering
		if (bShowX)
		{
			P.Rotation = AxisRots[0] * BaseRot;
			P.Color = XAxisId.GetColor();
			Renderer.RenderEditorPrimitive(P, RenderState);
		}

		// Y axis rendering
		if (bShowY)
		{
			P.Rotation = AxisRots[1] * BaseRot;
			P.Color = YAxisId.GetColor();
			Renderer.RenderEditorPrimitive(P, RenderState);
		}

		// Z axis rendering
		if (bShowZ)
		{
			P.Rotation = AxisRots[2] * BaseRot;
			P.Color = ZAxisId.GetColor();
			Renderer.RenderEditorPrimitive(P, RenderState);
		}

		// Center sphere rendering for Translate/Scale modes
		{
			const float SphereRadius = 0.10f * RenderScale;
			constexpr int NumSegments = 16;
			constexpr int NumRings = 16;

			TArray<FNormalVertex> vertices;
			TArray<uint32> indices;

			// Generate sphere mesh
			for (int ring = 0; ring <= NumRings; ++ring)
			{
				const float Theta = PI * static_cast<float>(ring) / static_cast<float>(NumRings);
				const float SinTheta = sinf(Theta);
				const float CosTheta = cosf(Theta);

				for (int seg = 0; seg <= NumSegments; ++seg)
				{
					const float Phi = 2.0f * PI * static_cast<float>(seg) / static_cast<float>(NumSegments);
					const float SinPhi = sinf(Phi);
					const float CosPhi = cosf(Phi);

					FNormalVertex v;
					v.Position.X = SphereRadius * SinTheta * CosPhi;
					v.Position.Y = SphereRadius * SinTheta * SinPhi;
					v.Position.Z = SphereRadius * CosTheta;
					v.Normal = v.Position.GetNormalized();
					v.Color = FVector4(1, 1, 1, 1);
					vertices.push_back(v);
				}
			}

			// Generate indices
			for (int ring = 0; ring < NumRings; ++ring)
			{
				for (int seg = 0; seg < NumSegments; ++seg)
				{
					const int Current = ring * (NumSegments + 1) + seg;
					const int Next = Current + NumSegments + 1;

					indices.push_back(Current);
					indices.push_back(Next);
					indices.push_back(Current + 1);

					indices.push_back(Current + 1);
					indices.push_back(Next);
					indices.push_back(Next + 1);
				}
			}

			if (!indices.empty())
			{
				ID3D11Buffer* sphereVB = nullptr;
				ID3D11Buffer* sphereIB = nullptr;
				FGizmoGeometry::CreateTempBuffers(vertices, indices, &sphereVB, &sphereIB);

				FEditorPrimitive SpherePrim = P;
				SpherePrim.VertexBuffer = sphereVB;
				SpherePrim.NumVertices = static_cast<uint32>(vertices.size());
				SpherePrim.IndexBuffer = sphereIB;
				SpherePrim.NumIndices = static_cast<uint32>(indices.size());
				SpherePrim.Rotation = FQuaternion::Identity();
				SpherePrim.Scale = FVector(1.0f, 1.0f, 1.0f);
				SpherePrim.Color = CenterId.GetColor();
				Renderer.RenderEditorPrimitive(SpherePrim, RenderState);

				sphereVB->Release();
				sphereIB->Release();
			}
		}
	}
}
