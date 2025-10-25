#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/Camera.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Actor/Public/Actor.h"
#include "Editor/Public/Editor.h"
#include "Global/Quaternion.h"

namespace
{
	// Quarter ring mesh generation constants
	constexpr int32 kQuarterRingSegments = 48;
	constexpr float kQuarterRingStartAngle = 0.0f;
	constexpr float kQuarterRingEndAngle = 3.14159265f / 2.0f;  // 90 degrees

	// Rotation finding tolerance
	constexpr float kDotProductParallelThreshold = 0.999f;
	constexpr float kDotProductOppositeThreshold = -0.999f;
	constexpr float kAxisLengthSquaredEpsilon = 0.001f;

	// Gizmo axis definitions
	// X ring: ZAxis × YAxis, Y ring: XAxis × ZAxis, Z ring: XAxis × YAxis
	const FVector kLocalAxis0[3] = {
		FVector(0.0f, 0.0f, 1.0f),  // X ring: Z axis
		FVector(1.0f, 0.0f, 0.0f),  // Y ring: X axis
		FVector(1.0f, 0.0f, 0.0f)   // Z ring: X axis
	};
	const FVector kLocalAxis1[3] = {
		FVector(0.0f, 1.0f, 0.0f),  // X ring: Y axis
		FVector(0.0f, 0.0f, 1.0f),  // Y ring: Z axis
		FVector(0.0f, 1.0f, 0.0f)   // Z ring: Y axis
	};
}

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

	/**
	 * @brief Translation Setting
	 */
	const float ScaleT = TranslateCollisionConfig.Scale;
	Primitives[0].VertexBuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
	Primitives[0].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
	Primitives[0].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[0].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[0].bShouldAlwaysVisible = true;

	/**
	 * @brief Rotation Setting
	 */
	Primitives[1].VertexBuffer = nullptr;  // 렌더링 시 동적으로 설정
	Primitives[1].NumVertices = 0;
	Primitives[1].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[1].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[1].bShouldAlwaysVisible = true;

	/**
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

/**
 * @brief 즉시 렌더링을 위한 임시 D3D11 버퍼 생성 (UE DrawThickArc 방식)
 * @param InVertices 정점 데이터 배열
 * @param InIndices 인덱스 데이터 배열
 * @param OutVB 생성된 정점 버퍼 (출력)
 * @param OutIB 생성된 인덱스 버퍼 (출력)
 * @note 버퍼는 D3D11_USAGE_IMMUTABLE로 생성되며, 렌더링 후 즉시 Release해야 함
 */
static void CreateTempBuffers(const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices,
	ID3D11Buffer** OutVB, ID3D11Buffer** OutIB)
{
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * InVertices.size());
	VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBInitData = {};
	VBInitData.pSysMem = InVertices.data();

	URenderer::GetInstance().GetDevice()->CreateBuffer(&VBDesc, &VBInitData, OutVB);

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * InIndices.size());
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBInitData = {};
	IBInitData.pSysMem = InIndices.data();

	URenderer::GetInstance().GetDevice()->CreateBuffer(&IBDesc, &IBInitData, OutIB);
}

/**
 * @brief 회전 각도를 시각화하는 Arc 메쉬를 동적으로 생성
 * @param Axis0 시작 방향 벡터
 * @param Axis1 90도 방향 벡터
 * @param InnerRadius 안쪽 반지름
 * @param OuterRadius 바깥 반지름
 * @param AngleInRadians 회전 각도 (라디안)
 * @param OutVertices 생성된 정점 배열 (출력)
 * @param OutIndices 생성된 인덱스 배열 (출력)
 */
static void GenerateRotationArcMesh(const FVector& Axis0, const FVector& Axis1,
	float InnerRadius, float OuterRadius, float AngleInRadians,
	TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	if (AngleInRadians <= 0.0f)
	{
		return;
	}

	// 세그먼트 수 계산 (각도에 비례)
	const float AngleRatio = std::abs(AngleInRadians) / (2.0f * 3.14159265f);
	const int32 NumPoints = std::max(2, static_cast<int32>(kQuarterRingSegments * 4 * AngleRatio)) + 1;

	// 회전축 계산 (Axis0 × Axis1)
	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	// 두 개의 링 생성 (Outer, Inner)
	for (int32 RadiusIndex = 0; RadiusIndex < 2; ++RadiusIndex)
	{
		const float Radius = (RadiusIndex == 0) ? OuterRadius : InnerRadius;

		// 각 정점 생성
		for (int32 VertexIndex = 0; VertexIndex <= NumPoints; ++VertexIndex)
		{
			const float Percent = static_cast<float>(VertexIndex) / static_cast<float>(NumPoints);
			const float Angle = Percent * std::abs(AngleInRadians);
			const float AngleDeg = FVector::GetRadianToDegree(Angle);

			// Axis0를 ZAxis 기준으로 회전
			const FQuaternion RotQuat = FQuaternion::FromAxisAngle(ZAxis, -FVector::GetDegreeToRadian(AngleDeg));
			FVector VertexDir = RotQuat.RotateVector(Axis0);
			VertexDir.Normalize();

			const FVector VertexPosition = VertexDir * Radius;
			FVector Normal = VertexPosition;
			Normal.Normalize();

			FNormalVertex Vertex;
			Vertex.Position = VertexPosition;
			Vertex.Normal = Normal;
			Vertex.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 노란색
			Vertex.TexCoord = FVector2(Percent, static_cast<float>(RadiusIndex));

			OutVertices.push_back(Vertex);
		}
	}

	// 삼각형 인덱스 생성 (Quad strip)
	const int32 InnerVertexStartIndex = NumPoints + 1;
	for (int32 VertexIndex = 0; VertexIndex < NumPoints; ++VertexIndex)
	{
		// 첫 번째 삼각형 (CCW)
		OutIndices.push_back(VertexIndex);
		OutIndices.push_back(VertexIndex + 1);
		OutIndices.push_back(InnerVertexStartIndex + VertexIndex);

		// 두 번째 삼각형 (CCW)
		OutIndices.push_back(VertexIndex + 1);
		OutIndices.push_back(InnerVertexStartIndex + VertexIndex + 1);
		OutIndices.push_back(InnerVertexStartIndex + VertexIndex);
	}
}

/**
 * @brief QuarterRing 메쉬를 동적으로 생성 (UE DrawThickArc 방식)
 * @param Axis0 시작 방향 벡터 (0도)
 * @param Axis1 끝 방향 벡터 (90도)
 * @param InnerRadius 안쪽 반지름
 * @param OuterRadius 바깥 반지름
 * @param OutVertices 생성된 정점 배열 (출력)
 * @param OutIndices 생성된 인덱스 배열 (출력)
 * @note Axis0에서 시작하여 Axis1까지 90도 호(arc)를 생성하며, 두께는 InnerRadius와 OuterRadius의 차이로 결정됨
 */
static void GenerateQuarterRingMesh(const FVector& Axis0, const FVector& Axis1,
	float InnerRadius, float OuterRadius,
	TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	// 세그먼트 수 계산
	const int32 NumPoints = static_cast<int32>(kQuarterRingSegments * (kQuarterRingEndAngle - kQuarterRingStartAngle) / (3.14159265f / 2.0f)) + 1;

	// 회전축 계산 (Axis0 × Axis1)
	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	// 두 개의 링 생성 (Outer, Inner)
	for (int32 RadiusIndex = 0; RadiusIndex < 2; ++RadiusIndex)
	{
		const float Radius = (RadiusIndex == 0) ? OuterRadius : InnerRadius;

		// 각 정점 생성
		for (int32 VertexIndex = 0; VertexIndex <= NumPoints; ++VertexIndex)
		{
			const float Percent = static_cast<float>(VertexIndex) / static_cast<float>(NumPoints);
			const float Angle = kQuarterRingStartAngle + Percent * (kQuarterRingEndAngle - kQuarterRingStartAngle);
			const float AngleDeg = FVector::GetRadianToDegree(Angle);

			// Axis0를 ZAxis 기준으로 회전
			const FQuaternion RotQuat = FQuaternion::FromAxisAngle(ZAxis, -FVector::GetDegreeToRadian(AngleDeg));
			FVector VertexDir = RotQuat.RotateVector(Axis0);
			VertexDir.Normalize();

			const FVector VertexPosition = VertexDir * Radius;
			FVector Normal = VertexPosition;
			Normal.Normalize();

			FNormalVertex Vertex;
			Vertex.Position = VertexPosition;
			Vertex.Normal = Normal;
			Vertex.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 노란색 (나중에 덮어씌워짐)
			Vertex.TexCoord = FVector2(Percent, static_cast<float>(RadiusIndex));

			OutVertices.push_back(Vertex);
		}
	}

	// 삼각형 인덱스 생성 (Quad strip)
	const int32 InnerVertexStartIndex = NumPoints + 1;
	for (int32 VertexIndex = 0; VertexIndex < NumPoints; ++VertexIndex)
	{
		// 첫 번째 삼각형 (CCW)
		OutIndices.push_back(VertexIndex);
		OutIndices.push_back(VertexIndex + 1);
		OutIndices.push_back(InnerVertexStartIndex + VertexIndex);

		// 두 번째 삼각형 (CCW)
		OutIndices.push_back(VertexIndex + 1);
		OutIndices.push_back(InnerVertexStartIndex + VertexIndex + 1);
		OutIndices.push_back(InnerVertexStartIndex + VertexIndex);
	}
}

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

	// 회전 모드 확인
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	bool bIsRotateMode = (GizmoMode == EGizmoMode::Rotate);

	// 각 링을 해당 평면으로 회전시키는 변환 (드래그 시 Ring 사용)
	FQuaternion AxisRots[3] = {
		FQuaternion::Identity(),  // X링: YZ 평면
		FQuaternion::FromAxisAngle(FVector::UpVector(), FVector::GetDegreeToRadian(-90.0f)),  // Y링: XZ 평면
		FQuaternion::FromAxisAngle(FVector::RightVector(), FVector::GetDegreeToRadian(90.0f))  // Z링: XY 평면
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
			// Top/Bottom 뷰 (XY 평면) → Z축 회전
			OrthoWorldAxis = EGizmoDirection::Up;
		}
		else if (AbsY > AbsX && AbsY > AbsZ)
		{
			// Left/Right 뷰 (XZ 평면) → Y축 회전
			OrthoWorldAxis = EGizmoDirection::Right;
		}
		else
		{
			// Front/Back 뷰 (YZ 평면) → X축 회전
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

	// X축 (Forward) - 빨간색
	if (bShowX)
	{
		if (bIsRotateMode && bIsDragging && GizmoDirection == EGizmoDirection::Forward)
		{
			// 드래그 중인 축: Full Ring + 회전 각도 Arc
			P.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
			P.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
			P.IndexBuffer = nullptr;
			P.NumIndices = 0;
			P.Rotation = AxisRots[0] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Forward);
			Renderer.RenderEditorPrimitive(P, RenderState);

			// 회전 각도 Arc 렌더링 (하이라이트)
			if (std::abs(CurrentRotationAngle) > 0.001f)
			{
				constexpr int AxisIndex = 0;
				const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : DragStartActorRotationQuat;

				FVector LocalAxis0 = bIsWorld ? kLocalAxis0[AxisIndex] : GizmoRot.RotateVector(kLocalAxis0[AxisIndex]);
				FVector LocalAxis1 = bIsWorld ? kLocalAxis1[AxisIndex] : GizmoRot.RotateVector(kLocalAxis1[AxisIndex]);

				FVector LocalRenderAxis0 = bIsWorld ? LocalAxis0 : GizmoRot.Inverse().RotateVector(LocalAxis0);
				FVector LocalRenderAxis1 = bIsWorld ? LocalAxis1 : GizmoRot.Inverse().RotateVector(LocalAxis1);
				LocalRenderAxis0.Normalize();
				LocalRenderAxis1.Normalize();

				TArray<FNormalVertex> arcVertices;
				TArray<uint32> arcIndices;
				GenerateRotationArcMesh(LocalRenderAxis0, LocalRenderAxis1,
					RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
					CurrentRotationAngle, arcVertices, arcIndices);

				if (!arcIndices.empty())
				{
					ID3D11Buffer* arcVB = nullptr;
					ID3D11Buffer* arcIB = nullptr;
					CreateTempBuffers(arcVertices, arcIndices, &arcVB, &arcIB);

					P.VertexBuffer = arcVB;
					P.NumVertices = static_cast<uint32>(arcVertices.size());
					P.IndexBuffer = arcIB;
					P.NumIndices = static_cast<uint32>(arcIndices.size());
					P.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
					Renderer.RenderEditorPrimitive(P, RenderState);

					arcVB->Release();
					arcIB->Release();
				}
			}
		}
		else if (bIsRotateMode)
		{
			if (bIsOrtho && bIsWorld)
			{
				// 오쏘 뷰 + World 모드: Full Ring
				P.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
				P.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
				P.IndexBuffer = nullptr;
				P.NumIndices = 0;
				P.Rotation = AxisRots[0] * BaseRot;
				P.Color = ColorFor(EGizmoDirection::Forward);
				Renderer.RenderEditorPrimitive(P, RenderState);
			}
			else
			{
				// 퍼스펙티브 또는 오쏘 뷰 Local 모드: QuarterRing
				constexpr int AxisIndex = 0;
				const FVector GizmoLoc = P.Location;
				const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
				const FVector CameraLoc = InCamera->GetLocation();
				const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

				FVector Axis0 = bIsWorld ? kLocalAxis0[AxisIndex] : GizmoRot.RotateVector(kLocalAxis0[AxisIndex]);
				FVector Axis1 = bIsWorld ? kLocalAxis1[AxisIndex] : GizmoRot.RotateVector(kLocalAxis1[AxisIndex]);

				const bool bMirrorAxis0 = (Axis0.Dot(DirectionToWidget) <= 0.0f);
				const bool bMirrorAxis1 = (Axis1.Dot(DirectionToWidget) <= 0.0f);
				const FVector RenderAxis0 = bMirrorAxis0 ? Axis0 : -Axis0;
				const FVector RenderAxis1 = bMirrorAxis1 ? Axis1 : -Axis1;

				FVector LocalRenderAxis0 = bIsWorld ? RenderAxis0 : GizmoRot.Inverse().RotateVector(RenderAxis0);
				FVector LocalRenderAxis1 = bIsWorld ? RenderAxis1 : GizmoRot.Inverse().RotateVector(RenderAxis1);
				LocalRenderAxis0.Normalize();
				LocalRenderAxis1.Normalize();

				TArray<FNormalVertex> vertices;
				TArray<uint32> indices;
				GenerateQuarterRingMesh(LocalRenderAxis0, LocalRenderAxis1,
					RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
					vertices, indices);

				ID3D11Buffer* tempVB = nullptr;
				ID3D11Buffer* tempIB = nullptr;
				CreateTempBuffers(vertices, indices, &tempVB, &tempIB);

				P.VertexBuffer = tempVB;
				P.NumVertices = static_cast<uint32>(vertices.size());
				P.IndexBuffer = tempIB;
				P.NumIndices = static_cast<uint32>(indices.size());
				P.Rotation = BaseRot;
				P.Color = ColorFor(EGizmoDirection::Forward);
				Renderer.RenderEditorPrimitive(P, RenderState);

				tempVB->Release();
				tempIB->Release();
			}
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
			// 드래그 중인 축: Full Ring + 회전 각도 Arc
			P.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
			P.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
			P.IndexBuffer = nullptr;
			P.NumIndices = 0;
			P.Rotation = AxisRots[1] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Right);
			Renderer.RenderEditorPrimitive(P, RenderState);

			// 회전 각도 Arc 렌더링
			if (std::abs(CurrentRotationAngle) > 0.001f)
			{
				constexpr int AxisIndex = 1;
				const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : DragStartActorRotationQuat;

				FVector LocalAxis0 = bIsWorld ? kLocalAxis0[AxisIndex] : GizmoRot.RotateVector(kLocalAxis0[AxisIndex]);
				FVector LocalAxis1 = bIsWorld ? kLocalAxis1[AxisIndex] : GizmoRot.RotateVector(kLocalAxis1[AxisIndex]);

				FVector LocalRenderAxis0 = bIsWorld ? LocalAxis0 : GizmoRot.Inverse().RotateVector(LocalAxis0);
				FVector LocalRenderAxis1 = bIsWorld ? LocalAxis1 : GizmoRot.Inverse().RotateVector(LocalAxis1);
				LocalRenderAxis0.Normalize();
				LocalRenderAxis1.Normalize();

				TArray<FNormalVertex> arcVertices;
				TArray<uint32> arcIndices;
				GenerateRotationArcMesh(LocalRenderAxis0, LocalRenderAxis1,
					RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
					CurrentRotationAngle, arcVertices, arcIndices);

				if (!arcIndices.empty())
				{
					ID3D11Buffer* arcVB = nullptr;
					ID3D11Buffer* arcIB = nullptr;
					CreateTempBuffers(arcVertices, arcIndices, &arcVB, &arcIB);

					P.VertexBuffer = arcVB;
					P.NumVertices = static_cast<uint32>(arcVertices.size());
					P.IndexBuffer = arcIB;
					P.NumIndices = static_cast<uint32>(arcIndices.size());
					P.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
					Renderer.RenderEditorPrimitive(P, RenderState);

					arcVB->Release();
					arcIB->Release();
				}
			}
		}
		else if (bIsRotateMode)
		{
			if (bIsOrtho && bIsWorld)
			{
				// 오쏘 뷰 + World 모드: Full Ring
				P.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
				P.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
				P.IndexBuffer = nullptr;
				P.NumIndices = 0;
				P.Rotation = AxisRots[1] * BaseRot;
				P.Color = ColorFor(EGizmoDirection::Right);
				Renderer.RenderEditorPrimitive(P, RenderState);
			}
			else
			{
				// 퍼스펙티브 또는 오쏘 뷰 Local 모드: QuarterRing
				constexpr int AxisIndex = 1;
				const FVector GizmoLoc = P.Location;
				const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
				const FVector CameraLoc = InCamera->GetLocation();
				const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

				FVector Axis0 = bIsWorld ? kLocalAxis0[AxisIndex] : GizmoRot.RotateVector(kLocalAxis0[AxisIndex]);
				FVector Axis1 = bIsWorld ? kLocalAxis1[AxisIndex] : GizmoRot.RotateVector(kLocalAxis1[AxisIndex]);

				const bool bMirrorAxis0 = (Axis0.Dot(DirectionToWidget) <= 0.0f);
				const bool bMirrorAxis1 = (Axis1.Dot(DirectionToWidget) <= 0.0f);
				const FVector RenderAxis0 = bMirrorAxis0 ? Axis0 : -Axis0;
				const FVector RenderAxis1 = bMirrorAxis1 ? Axis1 : -Axis1;

				FVector LocalRenderAxis0 = bIsWorld ? RenderAxis0 : GizmoRot.Inverse().RotateVector(RenderAxis0);
				FVector LocalRenderAxis1 = bIsWorld ? RenderAxis1 : GizmoRot.Inverse().RotateVector(RenderAxis1);
				LocalRenderAxis0.Normalize();
				LocalRenderAxis1.Normalize();

				TArray<FNormalVertex> vertices;
				TArray<uint32> indices;
				GenerateQuarterRingMesh(LocalRenderAxis0, LocalRenderAxis1,
					RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
					vertices, indices);

				ID3D11Buffer* tempVB = nullptr;
				ID3D11Buffer* tempIB = nullptr;
				CreateTempBuffers(vertices, indices, &tempVB, &tempIB);

				P.VertexBuffer = tempVB;
				P.NumVertices = static_cast<uint32>(vertices.size());
				P.IndexBuffer = tempIB;
				P.NumIndices = static_cast<uint32>(indices.size());
				P.Rotation = BaseRot;
				P.Color = ColorFor(EGizmoDirection::Right);
				Renderer.RenderEditorPrimitive(P, RenderState);

				tempVB->Release();
				tempIB->Release();
			}
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
			// 드래그 중인 축: Full Ring + 회전 각도 Arc
			P.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
			P.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
			P.IndexBuffer = nullptr;
			P.NumIndices = 0;
			P.Rotation = AxisRots[2] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Up);
			Renderer.RenderEditorPrimitive(P, RenderState);

			// 회전 각도 Arc 렌더링
			if (std::abs(CurrentRotationAngle) > 0.001f)
			{
				constexpr int AxisIndex = 2;
				const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : DragStartActorRotationQuat;

				FVector LocalAxis0 = bIsWorld ? kLocalAxis0[AxisIndex] : GizmoRot.RotateVector(kLocalAxis0[AxisIndex]);
				FVector LocalAxis1 = bIsWorld ? kLocalAxis1[AxisIndex] : GizmoRot.RotateVector(kLocalAxis1[AxisIndex]);

				FVector LocalRenderAxis0 = bIsWorld ? LocalAxis0 : GizmoRot.Inverse().RotateVector(LocalAxis0);
				FVector LocalRenderAxis1 = bIsWorld ? LocalAxis1 : GizmoRot.Inverse().RotateVector(LocalAxis1);
				LocalRenderAxis0.Normalize();
				LocalRenderAxis1.Normalize();

				TArray<FNormalVertex> arcVertices;
				TArray<uint32> arcIndices;
				GenerateRotationArcMesh(LocalRenderAxis0, LocalRenderAxis1,
					RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
					CurrentRotationAngle, arcVertices, arcIndices);

				if (!arcIndices.empty())
				{
					ID3D11Buffer* arcVB = nullptr;
					ID3D11Buffer* arcIB = nullptr;
					CreateTempBuffers(arcVertices, arcIndices, &arcVB, &arcIB);

					P.VertexBuffer = arcVB;
					P.NumVertices = static_cast<uint32>(arcVertices.size());
					P.IndexBuffer = arcIB;
					P.NumIndices = static_cast<uint32>(arcIndices.size());
					P.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
					Renderer.RenderEditorPrimitive(P, RenderState);

					arcVB->Release();
					arcIB->Release();
				}
			}
		}
		else if (bIsRotateMode)
		{
			if (bIsOrtho && bIsWorld)
			{
				// 오쏘 뷰 + World 모드: Full Ring
				P.VertexBuffer = AssetManager.GetVertexbuffer(EPrimitiveType::Ring);
				P.NumVertices = AssetManager.GetNumVertices(EPrimitiveType::Ring);
				P.IndexBuffer = nullptr;
				P.NumIndices = 0;
				P.Rotation = AxisRots[2] * BaseRot;
				P.Color = ColorFor(EGizmoDirection::Up);
				Renderer.RenderEditorPrimitive(P, RenderState);
			}
			else
			{
				// 퍼스펙티브 또는 오쏘 뷰 Local 모드: QuarterRing
				constexpr int AxisIndex = 2;
				const FVector GizmoLoc = P.Location;
				const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
				const FVector CameraLoc = InCamera->GetLocation();
				const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

				FVector Axis0 = bIsWorld ? kLocalAxis0[AxisIndex] : GizmoRot.RotateVector(kLocalAxis0[AxisIndex]);
				FVector Axis1 = bIsWorld ? kLocalAxis1[AxisIndex] : GizmoRot.RotateVector(kLocalAxis1[AxisIndex]);

				const bool bMirrorAxis0 = (Axis0.Dot(DirectionToWidget) <= 0.0f);
				const bool bMirrorAxis1 = (Axis1.Dot(DirectionToWidget) <= 0.0f);
				const FVector RenderAxis0 = bMirrorAxis0 ? Axis0 : -Axis0;
				const FVector RenderAxis1 = bMirrorAxis1 ? Axis1 : -Axis1;

				FVector LocalRenderAxis0 = bIsWorld ? RenderAxis0 : GizmoRot.Inverse().RotateVector(RenderAxis0);
				FVector LocalRenderAxis1 = bIsWorld ? RenderAxis1 : GizmoRot.Inverse().RotateVector(RenderAxis1);
				LocalRenderAxis0.Normalize();
				LocalRenderAxis1.Normalize();

				TArray<FNormalVertex> vertices;
				TArray<uint32> indices;
				GenerateQuarterRingMesh(LocalRenderAxis0, LocalRenderAxis1,
					RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
					vertices, indices);

				ID3D11Buffer* tempVB = nullptr;
				ID3D11Buffer* tempIB = nullptr;
				CreateTempBuffers(vertices, indices, &tempVB, &tempIB);

				P.VertexBuffer = tempVB;
				P.NumVertices = static_cast<uint32>(vertices.size());
				P.IndexBuffer = tempIB;
				P.NumIndices = static_cast<uint32>(indices.size());
				P.Rotation = BaseRot;
				P.Color = ColorFor(EGizmoDirection::Up);
				Renderer.RenderEditorPrimitive(P, RenderState);

				tempVB->Release();
				tempIB->Release();
			}
		}
		else
		{
			// Translate/Scale 모드
			P.Rotation = AxisRots[2] * BaseRot;
			P.Color = ColorFor(EGizmoDirection::Up);
			Renderer.RenderEditorPrimitive(P, RenderState);
		}
	}
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

/**
 * @brief 뷰포트별로 QuarterRing 시작/끝 방향을 즉시 계산하는 함수
 * @param InCamera 
 * @param InAxis 
 * @param OutStartDir 
 * @param OutEndDir 
 */
void UGizmo::CalculateQuarterRingDirections(UCamera* InCamera, EGizmoDirection InAxis,
                                            FVector& OutStartDir, FVector& OutEndDir) const
{
	if (!InCamera || !TargetComponent)
	{
		OutStartDir = FVector::ForwardVector();
		OutEndDir = FVector::RightVector();
		return;
	}

	const int Idx = AxisIndex(InAxis);
	const FVector GizmoLoc = Primitives[(int)GizmoMode].Location;
	const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	const FVector CameraLoc = InCamera->GetLocation();
	const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

	// 월드 공간 축 계산
	FVector Axis0 = bIsWorld ? kLocalAxis0[Idx] : GizmoRot.RotateVector(kLocalAxis0[Idx]);
	FVector Axis1 = bIsWorld ? kLocalAxis1[Idx] : GizmoRot.RotateVector(kLocalAxis1[Idx]);

	// 카메라 뷰에 기반한 축 뒤집기
	const bool bMirrorAxis0 = (Axis0.Dot(DirectionToWidget) <= 0.0f);
	const bool bMirrorAxis1 = (Axis1.Dot(DirectionToWidget) <= 0.0f);
	const FVector RenderAxis0 = bMirrorAxis0 ? Axis0 : -Axis0;
	const FVector RenderAxis1 = bMirrorAxis1 ? Axis1 : -Axis1;

	// 로컬 공간으로 변환
	FVector LocalRenderAxis0 = bIsWorld ? RenderAxis0 : GizmoRot.Inverse().RotateVector(RenderAxis0);
	FVector LocalRenderAxis1 = bIsWorld ? RenderAxis1 : GizmoRot.Inverse().RotateVector(RenderAxis1);
	LocalRenderAxis0.Normalize();
	LocalRenderAxis1.Normalize();

	// 월드 공간 방향으로 출력
	OutStartDir = bIsWorld ? LocalRenderAxis0 : GizmoRot.RotateVector(LocalRenderAxis0);
	OutEndDir = bIsWorld ? LocalRenderAxis1 : GizmoRot.RotateVector(LocalRenderAxis1);
}

/**
 * @brief 두 벡터 사이의 회전을 나타내는 쿼터니언을 계산 (Unreal Engine FQuat::FindBetween 동일)
 * @param From 시작 벡터
 * @param To 끝 벡터
 * @return From을 To로 회전시키는 쿼터니언
 */
// static FQuaternion FindRotationBetween(FVector From, FVector To)
// {
// 	From.Normalize();
// 	To.Normalize();
//
// 	const float Dot = From.Dot(To);
//
// 	// 두 벡터가 거의 평행한 경우
// 	if (Dot > kDotProductParallelThreshold)
// 	{
// 		return FQuaternion::Identity();
// 	}
//
// 	// 두 벡터가 거의 반대 방향인 경우
// 	if (Dot < kDotProductOppositeThreshold)
// 	{
// 		FVector Axis = FVector::RightVector().Cross(From);
// 		if (Axis.LengthSquared() < kAxisLengthSquaredEpsilon)
// 		{
// 			Axis = FVector::UpVector().Cross(From);
// 		}
// 		Axis.Normalize();
// 		return FQuaternion::FromAxisAngle(Axis, 3.14159265f);
// 	}
//
// 	FVector Axis = From.Cross(To);
// 	Axis.Normalize();
// 	const float Angle = std::acosf(Dot);
// 	return FQuaternion::FromAxisAngle(Axis, Angle);
// }
