#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Render/UI/Overlay/Public/D2DOverlayManager.h"
#include "Editor/Public/Camera.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Actor/Public/Actor.h"
#include "Editor/Public/Editor.h"
#include "Global/Quaternion.h"

IMPLEMENT_CLASS(UGizmo, UObject)

namespace
{
	// Quarter ring mesh generation constants
	constexpr int32 QuarterRingSegments = 48;
	constexpr float QuarterRingStartAngle = 0.0f;
	constexpr float QuarterRingEndAngle = PI / 2.0f;  // 90 degrees

	// Rotation finding tolerance
	// constexpr float kDotProductParallelThreshold = 0.999f;
	// constexpr float kDotProductOppositeThreshold = -0.999f;
	// constexpr float kAxisLengthSquaredEpsilon = 0.001f;

	// Gizmo axis definitions
	// X ring: ZAxis × YAxis, Y ring: XAxis × ZAxis, Z ring: XAxis × YAxis
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
 * @brief 원형 라인 메쉬 생성 (채워지지 않은 원)
 * @param Axis0 원 평면의 첫 번째 축 (예: X축 회전의 경우 Z축)
 * @param Axis1 원 평면의 두 번째 축 (예: X축 회전의 경우 Y축)
 * @param Radius 원의 반지름
 * @param Thickness 라인의 두께
 * @param OutVertices 생성된 정점 배열 (출력)
 * @param OutIndices 생성된 인덱스 배열 (출력)
 */
static void GenerateCircleLineMesh(const FVector& Axis0, const FVector& Axis1,
	float Radius, float Thickness,
	TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	constexpr int32 NumSegments = 64; // 원의 세그먼트 수
	constexpr int32 NumThicknessSegments = 6; // 두께 방향 세그먼트 수
	const float HalfThickness = Thickness * 0.5f;

	// 회전축 계산 (Arc/QuarterRing과 동일)
	FVector RotationAxis = Axis0.Cross(Axis1);
	RotationAxis.Normalize();

	// 원 둘레를 따라 회전하면서 원형 단면 생성
	for (int32 i = 0; i <= NumSegments; ++i)
	{
		const float Angle = (2.0f * PI * i) / NumSegments;

		// Axis0 방향으로 시작하여 회전
		const FQuaternion Rot = FQuaternion::FromAxisAngle(RotationAxis, -Angle);
		FVector CircleDir = Rot.RotateVector(Axis0);
		CircleDir.Normalize();

		// 원 둘레 상의 점
		const FVector CirclePoint = CircleDir * Radius;

		// 접선 방향
		FVector Tangent = RotationAxis.Cross(CircleDir);
		Tangent.Normalize();

		// 두께 방향 축들 (CircleDir과 Tangent로 원형 단면 생성)
		const FVector ThicknessAxis1 = CircleDir; // 반지름 방향
		const FVector ThicknessAxis2 = Tangent;    // 접선 방향

		// 원형 단면의 각 정점 생성
		for (int32 j = 0; j < NumThicknessSegments; ++j)
		{
			const float ThickAngle = (2.0f * PI * j) / NumThicknessSegments;
			const float CosThick = cosf(ThickAngle);
			const float SinThick = sinf(ThickAngle);

			const FVector Offset = ThicknessAxis1 * (HalfThickness * CosThick) + ThicknessAxis2 * (HalfThickness * SinThick);
			FNormalVertex v;
			v.Position = CirclePoint + Offset;
			v.Normal = Offset.GetNormalized();
			v.Color = FVector4(1, 1, 1, 1);
			OutVertices.push_back(v);
		}
	}

	// 인덱스 생성 (두께가 있는 튜브 형태)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		for (int32 j = 0; j < NumThicknessSegments; ++j)
		{
			const int32 Current = i * NumThicknessSegments + j;
			const int32 Next = i * NumThicknessSegments + ((j + 1) % NumThicknessSegments);
			const int32 NextRing = (i + 1) * NumThicknessSegments + j;
			const int32 NextRingNext = (i + 1) * NumThicknessSegments + ((j + 1) % NumThicknessSegments);

			OutIndices.push_back(Current);
			OutIndices.push_back(NextRing);
			OutIndices.push_back(Next);

			OutIndices.push_back(Next);
			OutIndices.push_back(NextRing);
			OutIndices.push_back(NextRingNext);
		}
	}
}

/**
 * @brief 회전 각도 표시용 3D Arc 메쉬 생성
 * @param Axis0 시작 방향 벡터
 * @param Axis1 회전 평면을 정의하는 두 번째 축
 * @param InnerRadius 안쪽 반지름
 * @param OuterRadius 바깥 반지름
 * @param Thickness 링의 3D 두께
 * @param AngleInRadians 회전 각도 (라디안, 양수/음수 모두 지원, 360도 이상 가능)
 * @param StartDirection Arc의 시작 방향 벡터 (기즈모 중심 기준)
 * @param OutVertices 생성된 정점 배열 (출력)
 * @param OutIndices 생성된 인덱스 배열 (출력)
 */
static void GenerateRotationArcMesh(const FVector& Axis0, const FVector& Axis1,
	float InnerRadius, float OuterRadius, float Thickness, float AngleInRadians,
	const FVector& StartDirection, TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	if (std::abs(AngleInRadians) < 0.001f)
	{
		return;
	}

	// 세그먼트 수 계산 (각도에 비례, 360도 이상도 지원)
	const float AbsAngle = std::abs(AngleInRadians);
	const float AngleRatio = AbsAngle / (2.0f * PI);
	const int32 NumArcPoints = std::max(2, static_cast<int32>(QuarterRingSegments * 4 * AngleRatio)) + 1;
	constexpr int32 NumThicknessSegments = 6; // 두께 방향 세그먼트 수

	// 회전축 계산 (Axis0 × Axis1)
	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	// StartDirection이 제공되면 사용, 아니면 Axis0에서 시작
	FVector StartAxis = Axis0;
	if (StartDirection.LengthSquared() > 0.001f)
	{
		// StartDirection을 회전 평면에 투영
		FVector ProjectedStart = StartDirection - ZAxis * StartDirection.Dot(ZAxis);
		if (ProjectedStart.LengthSquared() > 0.001f)
		{
			StartAxis = ProjectedStart.GetNormalized();
		}
	}
	const float SignedAngle = AngleInRadians;
	const float RotationDirection = (SignedAngle >= 0.0f) ? -1.0f : 1.0f;

	// 링의 중심 반지름
	const float MidRadius = (InnerRadius + OuterRadius) * 0.5f;
	const float RingWidth = (OuterRadius - InnerRadius) * 0.5f;
	const float HalfThickness = Thickness * 0.5f;

	// 호(arc) 방향과 두께 방향으로 정점 생성
	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints; ++ArcIdx)
	{
		const float ArcPercent = static_cast<float>(ArcIdx) / static_cast<float>(NumArcPoints - 1);
		const float CurrentAngle = ArcPercent * AbsAngle;
		const float CurrentAngleDeg = FVector::GetRadianToDegree(CurrentAngle);

		// 호 중심점의 방향
		const FQuaternion ArcRotQuat = FQuaternion::FromAxisAngle(ZAxis, RotationDirection * FVector::GetDegreeToRadian(CurrentAngleDeg));
		FVector ArcDir = ArcRotQuat.RotateVector(StartAxis);
		ArcDir.Normalize();

		// 호 중심점 위치
		const FVector ArcCenter = ArcDir * MidRadius;

		// 두께 방향 (ZAxis 방향)
		const FVector ThicknessAxis = ZAxis;

		// 너비 방향 (원의 반지름 방향)
		const FVector WidthAxis = ArcDir;

		// 두께 단면의 원형 정점들 생성
		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const float ThickAngle = (2.0f * PI * ThickIdx) / NumThicknessSegments;
			const float CosThick = std::cosf(ThickAngle);
			const float SinThick = std::sinf(ThickAngle);

			// 단면 원의 위치
			const FVector Offset = WidthAxis * (RingWidth * CosThick) + ThicknessAxis * (HalfThickness * SinThick);
			const FVector VertexPos = ArcCenter + Offset;

			// 노멀은 단면 원의 바깥 방향
			FVector Normal = Offset;
			Normal.Normalize();

			FNormalVertex Vertex;
			Vertex.Position = VertexPos;
			Vertex.Normal = Normal;
			Vertex.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			Vertex.TexCoord = FVector2(ArcPercent, static_cast<float>(ThickIdx) / NumThicknessSegments);

			OutVertices.push_back(Vertex);
		}
	}

	// 인덱스 생성
	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints - 1; ++ArcIdx)
	{
		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const int32 NextThickIdx = (ThickIdx + 1) % NumThicknessSegments;

			const int32 i0 = ArcIdx * NumThicknessSegments + ThickIdx;
			const int32 i1 = ArcIdx * NumThicknessSegments + NextThickIdx;
			const int32 i2 = (ArcIdx + 1) * NumThicknessSegments + ThickIdx;
			const int32 i3 = (ArcIdx + 1) * NumThicknessSegments + NextThickIdx;

			// 첫 번째 삼각형
			OutIndices.push_back(i0);
			OutIndices.push_back(i2);
			OutIndices.push_back(i1);

			// 두 번째 삼각형
			OutIndices.push_back(i1);
			OutIndices.push_back(i2);
			OutIndices.push_back(i3);
		}
	}
}

/**
 * @brief 회전 각도 눈금 메쉬 생성
 * @param Axis0 시작 방향 벡터
 * @param Axis1 회전 평면을 정의하는 두 번째 축
 * @param InnerRadius 링의 안쪽 반지름
 * @param OuterRadius 링의 바깥쪽 반지름
 * @param Thickness 눈금의 두께
 * @param OutVertices 생성된 정점 배열 (출력)
 * @param OutIndices 생성된 인덱스 배열 (출력)
 */
static void GenerateAngleTickMarks(const FVector& Axis0, const FVector& Axis1,
	float InnerRadius, float OuterRadius, float Thickness, float SnapAngleDegrees,
	TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	// 회전축 계산
	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	uint32 BaseVertexIndex = 0;

	// 360도 눈금 생성 (SnapAngleDegrees마다)
	int32 AngleStep = static_cast<int32>(SnapAngleDegrees);
	if (AngleStep <= 0)
	{
		AngleStep = 10;
		UViewportManager::GetInstance().SetRotationSnapAngle(static_cast<float>(AngleStep));
	}

	for (int32 Degree = 0; Degree < 360; Degree += AngleStep)
	{
		const float AngleRad = FVector::GetDegreeToRadian(static_cast<float>(Degree));

		// 90도마다 큰 눈금, 나머지는 작은 눈금
		const bool bIsLargeTick = (Degree % 90 == 0);
		// 링 범위 내에서만 렌더링 (큰 눈금: OuterRadius에서 InnerRadius까지, 작은 눈금: 더 짧게)
		const float TickStartRadius = bIsLargeTick ? OuterRadius * 1.00f : OuterRadius * 0.95f;
		const float TickEndRadius = bIsLargeTick ? InnerRadius * 1.00f : InnerRadius * 1.05f;
		const float TickThickness = Thickness * (bIsLargeTick ? 0.8f : 0.5f);

		// 눈금 방향
		const FQuaternion RotQuat = FQuaternion::FromAxisAngle(ZAxis, -AngleRad);
		FVector TickDir = RotQuat.RotateVector(Axis0);
		TickDir.Normalize();

		// 눈금의 양 끝점
		const FVector TickStart = TickDir * TickStartRadius;
		const FVector TickEnd = TickDir * TickEndRadius;

		// 눈금 두께 방향 (ZAxis 방향)
		const FVector ThicknessOffset = ZAxis * (TickThickness * 0.5f);

		// 4개 정점 (사각형 단면)
		FNormalVertex v0, v1, v2, v3;
		v0.Position = TickStart + ThicknessOffset;
		v1.Position = TickStart - ThicknessOffset;
		v2.Position = TickEnd + ThicknessOffset;
		v3.Position = TickEnd - ThicknessOffset;

		// 노멀 (반지름 방향)
		FVector Normal = TickDir;
		v0.Normal = v1.Normal = v2.Normal = v3.Normal = Normal;

		// 색상 (노란색)
		const FVector4 TickColor(1.0f, 1.0f, 0.0f, 1.0f);
		v0.Color = v1.Color = v2.Color = v3.Color = TickColor;

		OutVertices.push_back(v0);
		OutVertices.push_back(v1);
		OutVertices.push_back(v2);
		OutVertices.push_back(v3);

		// 인덱스 (2개 삼각형)
		OutIndices.push_back(BaseVertexIndex + 0);
		OutIndices.push_back(BaseVertexIndex + 2);
		OutIndices.push_back(BaseVertexIndex + 1);

		OutIndices.push_back(BaseVertexIndex + 1);
		OutIndices.push_back(BaseVertexIndex + 2);
		OutIndices.push_back(BaseVertexIndex + 3);

		BaseVertexIndex += 4;
	}
}

/**
 * @brief QuarterRing 메쉬를 3D 토러스 형태로 생성
 * @param Axis0 시작 방향 벡터 (0도)
 * @param Axis1 끝 방향 벡터 (90도)
 * @param InnerRadius 안쪽 반지름
 * @param OuterRadius 바깥 반지름
 * @param Thickness 링의 3D 두께
 * @param OutVertices 생성된 정점 배열 (출력)
 * @param OutIndices 생성된 인덱스 배열 (출력)
 */
static void GenerateQuarterRingMesh(const FVector& Axis0, const FVector& Axis1,
	float InnerRadius, float OuterRadius, float Thickness,
	TArray<FNormalVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	OutVertices.clear();
	OutIndices.clear();

	// 세그먼트 수
	constexpr int32 NumArcPoints = static_cast<int32>(QuarterRingSegments * (QuarterRingEndAngle - QuarterRingStartAngle) / (PI / 2.0f)) + 1;
	constexpr int32 NumThicknessSegments = 6; // 두께 방향 세그먼트 수

	// 회전축 계산 (Axis0 × Axis1)
	FVector ZAxis = Axis0.Cross(Axis1);
	ZAxis.Normalize();

	// 링의 중심 반지름
	const float MidRadius = (InnerRadius + OuterRadius) * 0.5f;
	const float RingWidth = (OuterRadius - InnerRadius) * 0.5f;
	const float HalfThickness = Thickness * 0.5f;

	// 호(arc) 방향과 두께 방향으로 정점 생성
	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints; ++ArcIdx)
	{
		const float ArcPercent = static_cast<float>(ArcIdx) / static_cast<float>(NumArcPoints - 1);
		const float ArcAngle = QuarterRingStartAngle + ArcPercent * (QuarterRingEndAngle - QuarterRingStartAngle);
		const float ArcAngleDeg = FVector::GetRadianToDegree(ArcAngle);

		// 호 중심점의 방향
		const FQuaternion ArcRotQuat = FQuaternion::FromAxisAngle(ZAxis, -FVector::GetDegreeToRadian(ArcAngleDeg));
		FVector ArcDir = ArcRotQuat.RotateVector(Axis0);
		ArcDir.Normalize();

		// 호 중심점 위치
		const FVector ArcCenter = ArcDir * MidRadius;

		// 두께 방향 (ZAxis 방향)
		const FVector ThicknessAxis = ZAxis;

		// 너비 방향 (원의 반지름 방향)
		const FVector WidthAxis = ArcDir;

		// 두께 단면의 원형 정점들 생성
		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const float ThickAngle = (2.0f * PI * ThickIdx) / NumThicknessSegments;
			const float CosThick = std::cosf(ThickAngle);
			const float SinThick = std::sinf(ThickAngle);

			// 단면 원의 위치 (WidthAxis와 ThicknessAxis로 정의되는 평면에서 원)
			const FVector Offset = WidthAxis * (RingWidth * CosThick) + ThicknessAxis * (HalfThickness * SinThick);
			const FVector VertexPos = ArcCenter + Offset;

			// 노멀은 단면 원의 바깥 방향
			FVector Normal = Offset;
			Normal.Normalize();

			FNormalVertex Vertex;
			Vertex.Position = VertexPos;
			Vertex.Normal = Normal;
			Vertex.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			Vertex.TexCoord = FVector2(ArcPercent, static_cast<float>(ThickIdx) / NumThicknessSegments);

			OutVertices.push_back(Vertex);
		}
	}

	// 인덱스 생성 (각 호 세그먼트를 따라 원형 단면을 연결)
	for (int32 ArcIdx = 0; ArcIdx < NumArcPoints - 1; ++ArcIdx)
	{
		for (int32 ThickIdx = 0; ThickIdx < NumThicknessSegments; ++ThickIdx)
		{
			const int32 NextThickIdx = (ThickIdx + 1) % NumThicknessSegments;

			const int32 i0 = ArcIdx * NumThicknessSegments + ThickIdx;
			const int32 i1 = ArcIdx * NumThicknessSegments + NextThickIdx;
			const int32 i2 = (ArcIdx + 1) * NumThicknessSegments + ThickIdx;
			const int32 i3 = (ArcIdx + 1) * NumThicknessSegments + NextThickIdx;

			// 첫 번째 삼각형
			OutIndices.push_back(i0);
			OutIndices.push_back(i2);
			OutIndices.push_back(i1);

			// 두 번째 삼각형
			OutIndices.push_back(i1);
			OutIndices.push_back(i2);
			OutIndices.push_back(i3);
		}
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
	PreviousMouseLocation = CollisionPoint;  // 누적 각도 계산을 위한 초기화

	// 스크린 공간 드래그 초기화 (뷰포트 로컬 좌표)
	POINT MousePos;
	GetCursorPos(&MousePos);
	ScreenToClient(GetActiveWindow(), &MousePos);

	const FRect& ViewportRect = UViewportManager::GetInstance().GetActiveViewportRect();
	const FVector2 CurrentScreenPos(
		static_cast<float>(MousePos.x) - static_cast<float>(ViewportRect.Left),
		static_cast<float>(MousePos.y) - static_cast<float>(ViewportRect.Top)
	);
	PreviousScreenPos = CurrentScreenPos;
	DragStartScreenPos = CurrentScreenPos;

	// 드래그 시작 방향 계산 (Arc 렌더링의 시작점으로 사용)
	FVector GizmoCenter = Primitives[static_cast<int>(GizmoMode)].Location;
	DragStartDirection = (CollisionPoint - GizmoCenter).GetNormalized();

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
	bool bIsHighlight = (InAxis == GizmoDirection);

	// 평면 기즈모가 선택되었을 때만 해당 평면의 2개 축도 하이라이트
	// (단일 축 선택 시에는 평면이 하이라이트되지 않음)

	// InAxis가 평면인 경우: GizmoDirection도 평면이어야만 하이라이트
	bool bIsInAxisPlane = (InAxis == EGizmoDirection::XY_Plane ||
	                       InAxis == EGizmoDirection::XZ_Plane ||
	                       InAxis == EGizmoDirection::YZ_Plane);

	if (!bIsHighlight && IsPlaneDirection() && !bIsInAxisPlane)
	{
		// 평면이 선택되었고, InAxis가 단일 축인 경우: 해당 평면의 축들을 하이라이트
		if (GizmoDirection == EGizmoDirection::XY_Plane)
		{
			// XY 평면: X(Forward, 빨강) + Y(Right, 초록)
			bIsHighlight = (InAxis == EGizmoDirection::Forward || InAxis == EGizmoDirection::Right);
		}
		else if (GizmoDirection == EGizmoDirection::XZ_Plane)
		{
			// XZ 평면: X(Forward, 빨강) + Z(Up, 파랑)
			bIsHighlight = (InAxis == EGizmoDirection::Forward || InAxis == EGizmoDirection::Up);
		}
		else if (GizmoDirection == EGizmoDirection::YZ_Plane)
		{
			// YZ 평면: Y(Right, 초록) + Z(Up, 파랑)
			bIsHighlight = (InAxis == EGizmoDirection::Right || InAxis == EGizmoDirection::Up);
		}
	}

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

	float Scale;

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
		ProjectedDepth = max(ProjectedDepth, MinDepth);

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
	const FVector GizmoLoc = Primitives[static_cast<int>(GizmoMode)].Location;
	const FQuaternion GizmoRot = bIsWorld ? FQuaternion::Identity() : TargetComponent->GetWorldRotationAsQuaternion();
	const FVector CameraLoc = InCamera->GetLocation();
	const FVector DirectionToWidget = (GizmoLoc - CameraLoc).GetNormalized();

	// 월드 공간 축 계산
	FVector Axis0 = bIsWorld ? LocalAxis0[Idx] : GizmoRot.RotateVector(LocalAxis0[Idx]);
	FVector Axis1 = bIsWorld ? LocalAxis1[Idx] : GizmoRot.RotateVector(LocalAxis1[Idx]);

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

void UGizmo::CollectRotationAngleOverlay(FD2DOverlayManager& Manager, UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (!bIsDragging || GizmoMode != EGizmoMode::Rotate || !TargetComponent)
	{
		return;
	}

	const FVector GizmoLocation = GetGizmoLocation();
	const FVector LocalGizmoAxis = GetGizmoAxis();
	const FQuaternion StartRotQuat = GetDragStartActorRotationQuat();

	// 월드 공간 회전축
	FVector WorldRotationAxis = LocalGizmoAxis;
	if (!IsWorldMode())
	{
		WorldRotationAxis = StartRotQuat.RotateVector(LocalGizmoAxis);
	}

	// 현재 회전 각도 계산 (스냅 적용)
	float DisplayAngleRadians = GetCurrentRotationAngle();
	if (UViewportManager::GetInstance().IsRotationSnapEnabled())
	{
		const float SnapAngleDegrees = UViewportManager::GetInstance().GetRotationSnapAngle();
		const float SnapAngleRadians = FVector::GetDegreeToRadian(SnapAngleDegrees);
		DisplayAngleRadians = std::round(GetCurrentRotationAngle() / SnapAngleRadians) * SnapAngleRadians;
	}

	// 렌더링과 동일하게 모든 축을 YZ 평면 기준으로 계산 후 회전 변환 적용
	// GenerateCircleLineMesh(Z, Y)는 Z에서 시작하여 Z x Y = X축 주위로 회전
	const FVector BaseAxis0 = FVector(0, 0, 1);  // Z (시작점)
	const FVector BaseAxis1 = FVector(0, 1, 0);  // Y

	// 각 축을 해당 평면으로 회전시키는 변환 (RenderGizmo의 AxisRots와 동일)
	FQuaternion AxisRotation;
	switch (GizmoDirection)
	{
	case EGizmoDirection::Forward:  // X축: YZ 평면 그대로
		AxisRotation = FQuaternion::Identity();
		break;
	case EGizmoDirection::Right:    // Y축: Z축 중심 -90도 회전 (YZ -> XZ)
		AxisRotation = FQuaternion::FromAxisAngle(FVector(0, 0, 1), FVector::GetDegreeToRadian(-90.0f));
		break;
	case EGizmoDirection::Up:       // Z축: Y축 중심 90도 회전 (YZ -> XY)
		AxisRotation = FQuaternion::FromAxisAngle(FVector(0, 1, 0), FVector::GetDegreeToRadian(90.0f));
		break;
	default:
		return;
	}

	// Local 모드면 컴포넌트 회전 적용
	FQuaternion TotalRotation = AxisRotation;
	if (!IsWorldMode())
	{
		TotalRotation = AxisRotation * StartRotQuat;
	}

	// YZ 평면에서 각도 위치의 방향 계산 (Z에서 시작)
	const FVector LocalDirection = BaseAxis0 * cosf(DisplayAngleRadians) + BaseAxis1 * sinf(DisplayAngleRadians);

	// 월드 공간으로 변환
	const FVector AngleDirection = TotalRotation.RotateVector(LocalDirection);

	// 회전 반지름 (기즈모 스케일 적용)
	const float RotateRadius = GetRotateOuterRadius();

	// 원 위의 점 (월드 공간)
	const FVector PointOnCircle = GizmoLocation + AngleDirection * RotateRadius;

	// 스크린 공간으로 투영
	const FCameraConstants& CamConst = InCamera->GetFViewProjConstants();
	const FMatrix ViewProj = CamConst.View * CamConst.Projection;

	FVector4 GizmoScreenPos4 = FVector4(GizmoLocation, 1.0f) * ViewProj;
	FVector4 PointScreenPos4 = FVector4(PointOnCircle, 1.0f) * ViewProj;

	if (GizmoScreenPos4.W <= 0.0f || PointScreenPos4.W <= 0.0f)
	{
		return;
	}

	GizmoScreenPos4 *= 1.0f / GizmoScreenPos4.W;
	PointScreenPos4 *= 1.0f / PointScreenPos4.W;

	// NDC -> 스크린 좌표
	const FVector2 GizmoScreenPos(
		(GizmoScreenPos4.X * 0.5f + 0.5f) * InViewport.Width + InViewport.TopLeftX,
		((-GizmoScreenPos4.Y) * 0.5f + 0.5f) * InViewport.Height + InViewport.TopLeftY
	);

	const FVector2 PointScreenPos(
		(PointScreenPos4.X * 0.5f + 0.5f) * InViewport.Width + InViewport.TopLeftX,
		((-PointScreenPos4.Y) * 0.5f + 0.5f) * InViewport.Height + InViewport.TopLeftY
	);

	// 기즈모 중심에서 점으로 향하는 방향
	FVector2 DirectionToPoint = PointScreenPos - GizmoScreenPos;
	const float DistToPoint = DirectionToPoint.Length();
	if (DistToPoint < 0.01f)
	{
		return;
	}
	DirectionToPoint = DirectionToPoint.GetNormalized();

	// 텍스트 위치 (뷰포트 크기 비례 원 바깥으로 추가 오프셋)
	// 스크린 공간 원 반지름에 비례하는 오프셋 계산
	constexpr float ScreenRadiusRatio = 0.3f;
	const float TextOffset = DistToPoint * ScreenRadiusRatio;
	const float TextX = PointScreenPos.X + DirectionToPoint.X * TextOffset;
	const float TextY = PointScreenPos.Y + DirectionToPoint.Y * TextOffset;

	// 텍스트 포맷 (UI와 일치하도록 각도 표시)
	// 회전 입력 계산과 부호 일치시키기
	float DisplayAngleDegrees = FVector::GetRadianToDegree(DisplayAngleRadians);

	// X, Y축은 Editor.cpp의 Axis0/Axis1 정의와 부호가 반대이므로 보정
	if (GizmoDirection == EGizmoDirection::Forward || GizmoDirection == EGizmoDirection::Right)
	{
		DisplayAngleDegrees = -DisplayAngleDegrees;
	}

	wchar_t AngleText[32];
	(void)swprintf_s(AngleText, L"%.1f°", DisplayAngleDegrees);

	// 텍스트 박스 (중앙 정렬)
	constexpr float TextBoxWidth = 60.0f;
	constexpr float TextBoxHeight = 20.0f;
	D2D1_RECT_F TextRect = D2D1::RectF(
		TextX - TextBoxWidth * 0.5f,
		TextY - TextBoxHeight * 0.5f,
		TextX + TextBoxWidth * 0.5f,
		TextY + TextBoxHeight * 0.5f
	);

	// 반투명 회색 배경
	const D2D1_COLOR_F ColorGrayBG = D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.7f);
	Manager.AddRectangle(TextRect, ColorGrayBG, true);

	// 노란색 텍스트
	const D2D1_COLOR_F ColorYellow = D2D1::ColorF(1.0f, 1.0f, 0.0f);
	Manager.AddText(AngleText, TextRect, ColorYellow, 15.0f, true, true, L"Consolas");
}

/**
 * @brief Translation 및 Scale 모드의 중심 구체 렌더링
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderCenterSphere(const FEditorPrimitive& P, float RenderScale)
{
	URenderer& Renderer = URenderer::GetInstance();

	const float SphereRadius = 0.07f * RenderScale;
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

	ID3D11Buffer* TempVB = nullptr;
	ID3D11Buffer* TempIB = nullptr;

	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.Usage = D3D11_USAGE_DEFAULT;
	VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * vertices.size());
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA VBData = {};
	VBData.pSysMem = vertices.data();
	Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.Usage = D3D11_USAGE_DEFAULT;
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = Indices.data();
	Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

	FEditorPrimitive Prim;
	Prim.Location = P.Location;
	Prim.Scale = FVector(1.0f, 1.0f, 1.0f);
	Prim.Rotation = FQuaternion::Identity();
	Prim.VertexBuffer = TempVB;
	Prim.NumVertices = static_cast<uint32>(vertices.size());
	Prim.IndexBuffer = TempIB;
	Prim.NumIndices = static_cast<uint32>(Indices.size());
	Prim.bShouldAlwaysVisible = true;
	Prim.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 중심 구체 색상: 흰색 또는 하이라이트
	bool bIsCenterSelected = (GizmoDirection == EGizmoDirection::Center);
	if (bIsCenterSelected && bIsDragging)
	{
		Prim.Color = FVector4(0.8f, 0.8f, 0.0f, 1.0f);  // 드래그 중: 짙은 노란색
	}
	else if (bIsCenterSelected)
	{
		Prim.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);  // 호버 중: 밝은 노란색
	}
	else
	{
		Prim.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);  // 기본: 흰색
	}

	Renderer.RenderEditorPrimitive(Prim, RenderState);

	TempVB->Release();
	TempIB->Release();
}

/**
 * @brief Translation 모드의 평면 기즈모 렌더링 (직각 표시)
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param BaseRot 기즈모 회전 (World/Local 모드)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderTranslatePlanes(const FEditorPrimitive& P, const FQuaternion& BaseRot, float RenderScale)
{
	URenderer& Renderer = URenderer::GetInstance();

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

		// 선분 1
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

			ID3D11Buffer* TempVB = nullptr;
			ID3D11Buffer* TempIB = nullptr;

			D3D11_BUFFER_DESC VBDesc = {};
			VBDesc.Usage = D3D11_USAGE_DEFAULT;
			VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * vertices.size());
			VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA VBData = {};
			VBData.pSysMem = vertices.data();
			Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

			D3D11_BUFFER_DESC IBDesc = {};
			IBDesc.Usage = D3D11_USAGE_DEFAULT;
			IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
			IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			D3D11_SUBRESOURCE_DATA IBData = {};
			IBData.pSysMem = Indices.data();
			Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

			FEditorPrimitive Prim1;
			Prim1.Location = P.Location;
			Prim1.Scale = FVector(1.0f, 1.0f, 1.0f);
			Prim1.Rotation = FQuaternion::Identity();
			Prim1.VertexBuffer = TempVB;
			Prim1.NumVertices = static_cast<uint32>(vertices.size());
			Prim1.IndexBuffer = TempIB;
			Prim1.NumIndices = static_cast<uint32>(Indices.size());
			Prim1.bShouldAlwaysVisible = true;
			Prim1.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			FVector4 Seg1BaseColor = GizmoColor[AxisIndex(Seg1Color)];
			if (bIsPlaneSelected && bIsDragging)
			{
				Prim1.Color = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
			}
			else if (bIsPlaneSelected)
			{
				Prim1.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			}
			else
			{
				Prim1.Color = Seg1BaseColor;
			}

			Renderer.RenderEditorPrimitive(Prim1, RenderState);

			TempVB->Release();
			TempIB->Release();
		}

		// 선분 2
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

			ID3D11Buffer* TempVB = nullptr;
			ID3D11Buffer* TempIB = nullptr;

			D3D11_BUFFER_DESC VBDesc = {};
			VBDesc.Usage = D3D11_USAGE_DEFAULT;
			VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * vertices.size());
			VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA VBData = {};
			VBData.pSysMem = vertices.data();
			Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

			D3D11_BUFFER_DESC IBDesc = {};
			IBDesc.Usage = D3D11_USAGE_DEFAULT;
			IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
			IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			D3D11_SUBRESOURCE_DATA IBData = {};
			IBData.pSysMem = Indices.data();
			Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

			FEditorPrimitive Prim2;
			Prim2.Location = P.Location;
			Prim2.Scale = FVector(1.0f, 1.0f, 1.0f);
			Prim2.Rotation = FQuaternion::Identity();
			Prim2.VertexBuffer = TempVB;
			Prim2.NumVertices = static_cast<uint32>(vertices.size());
			Prim2.IndexBuffer = TempIB;
			Prim2.NumIndices = static_cast<uint32>(Indices.size());
			Prim2.bShouldAlwaysVisible = true;
			Prim2.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			FVector4 Seg2BaseColor = GizmoColor[AxisIndex(Seg2Color)];
			if (bIsPlaneSelected && bIsDragging)
			{
				Prim2.Color = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
			}
			else if (bIsPlaneSelected)
			{
				Prim2.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			}
			else
			{
				Prim2.Color = Seg2BaseColor;
			}

			Renderer.RenderEditorPrimitive(Prim2, RenderState);

			TempVB->Release();
			TempIB->Release();
		}
	}
}

/**
 * @brief Scale 모드의 평면 기즈모 렌더링 (대각선 형태)
 * @param P 기즈모 Primitive (Location 정보 사용)
 * @param BaseRot 기즈모 회전 (항상 로컬 좌표)
 * @param RenderScale 스크린 공간 스케일
 */
void UGizmo::RenderScalePlanes(const FEditorPrimitive& P, const FQuaternion& BaseRot, float RenderScale)
{
	URenderer& Renderer = URenderer::GetInstance();

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

		// 선분 1
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

			ID3D11Buffer* TempVB = nullptr;
			ID3D11Buffer* TempIB = nullptr;

			D3D11_BUFFER_DESC VBDesc = {};
			VBDesc.Usage = D3D11_USAGE_DEFAULT;
			VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * vertices.size());
			VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA VBData = {};
			VBData.pSysMem = vertices.data();
			Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

			D3D11_BUFFER_DESC IBDesc = {};
			IBDesc.Usage = D3D11_USAGE_DEFAULT;
			IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
			IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			D3D11_SUBRESOURCE_DATA IBData = {};
			IBData.pSysMem = Indices.data();
			Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

			FEditorPrimitive Prim1;
			Prim1.Location = P.Location;
			Prim1.Scale = FVector(1.0f, 1.0f, 1.0f);
			Prim1.Rotation = FQuaternion::Identity();
			Prim1.VertexBuffer = TempVB;
			Prim1.NumVertices = static_cast<uint32>(vertices.size());
			Prim1.IndexBuffer = TempIB;
			Prim1.NumIndices = static_cast<uint32>(Indices.size());
			Prim1.bShouldAlwaysVisible = true;
			Prim1.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			FVector4 Seg1BaseColor = GizmoColor[AxisIndex(Seg1Color)];
			if (bIsPlaneSelected && bIsDragging)
			{
				Prim1.Color = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
			}
			else if (bIsPlaneSelected)
			{
				Prim1.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			}
			else
			{
				Prim1.Color = Seg1BaseColor;
			}

			Renderer.RenderEditorPrimitive(Prim1, RenderState);

			TempVB->Release();
			TempIB->Release();
		}

		// 선분 2
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

			ID3D11Buffer* TempVB = nullptr;
			ID3D11Buffer* TempIB = nullptr;

			D3D11_BUFFER_DESC VBDesc = {};
			VBDesc.Usage = D3D11_USAGE_DEFAULT;
			VBDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalVertex) * vertices.size());
			VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA VBData = {};
			VBData.pSysMem = vertices.data();
			Renderer.GetDevice()->CreateBuffer(&VBDesc, &VBData, &TempVB);

			D3D11_BUFFER_DESC IBDesc = {};
			IBDesc.Usage = D3D11_USAGE_DEFAULT;
			IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
			IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			D3D11_SUBRESOURCE_DATA IBData = {};
			IBData.pSysMem = Indices.data();
			Renderer.GetDevice()->CreateBuffer(&IBDesc, &IBData, &TempIB);

			FEditorPrimitive Prim2;
			Prim2.Location = P.Location;
			Prim2.Scale = FVector(1.0f, 1.0f, 1.0f);
			Prim2.Rotation = FQuaternion::Identity();
			Prim2.VertexBuffer = TempVB;
			Prim2.NumVertices = static_cast<uint32>(vertices.size());
			Prim2.IndexBuffer = TempIB;
			Prim2.NumIndices = static_cast<uint32>(Indices.size());
			Prim2.bShouldAlwaysVisible = true;
			Prim2.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			FVector4 Seg2BaseColor = GizmoColor[AxisIndex(Seg2Color)];
			if (bIsPlaneSelected && bIsDragging)
			{
				Prim2.Color = FVector4(0.8f, 0.8f, 0.0f, 1.0f);
			}
			else if (bIsPlaneSelected)
			{
				Prim2.Color = FVector4(1.0f, 1.0f, 0.0f, 1.0f);
			}
			else
			{
				Prim2.Color = Seg2BaseColor;
			}

			Renderer.RenderEditorPrimitive(Prim2, RenderState);

			TempVB->Release();
			TempIB->Release();
		}
	}
}

void UGizmo::RenderRotationCircles(const FEditorPrimitive& P, const FQuaternion& AxisRotation,
	const FQuaternion& BaseRot, const FVector4& AxisColor)
{
	URenderer& Renderer = URenderer::GetInstance();

	// Inner circle: 두꺼운 축 색상 선
	TArray<FNormalVertex> innerVertices, outerVertices;
	TArray<uint32> innerIndices, outerIndices;

	const float InnerLineThickness = RotateCollisionConfig.Thickness * 2.0f;
	GenerateCircleLineMesh(FVector(0, 0, 1), FVector(0, 1, 0),
		RotateCollisionConfig.InnerRadius, InnerLineThickness, innerVertices, innerIndices);

	if (!innerIndices.empty())
	{
		ID3D11Buffer* innerVB = nullptr;
		ID3D11Buffer* innerIB = nullptr;
		CreateTempBuffers(innerVertices, innerIndices, &innerVB, &innerIB);

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
	GenerateCircleLineMesh(FVector(0, 0, 1), FVector(0, 1, 0),
		RotateCollisionConfig.OuterRadius, OuterLineThickness, outerVertices, outerIndices);

	if (!outerIndices.empty())
	{
		ID3D11Buffer* outerVB = nullptr;
		ID3D11Buffer* outerIB = nullptr;
		CreateTempBuffers(outerVertices, outerIndices, &outerVB, &outerIB);

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
		GenerateAngleTickMarks(FVector(0, 0, 1), FVector(0, 1, 0),
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius,
			RotateCollisionConfig.Thickness, SnapAngle, tickVertices, tickIndices);

		if (!tickIndices.empty())
		{
			ID3D11Buffer* tickVB = nullptr;
			ID3D11Buffer* tickIB = nullptr;
			CreateTempBuffers(tickVertices, tickIndices, &tickVB, &tickIB);

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

		GenerateRotationArcMesh(FVector(0, 0, 1), FVector(0, 1, 0),
			ArcInnerRadius, ArcOuterRadius, RotateCollisionConfig.Thickness,
			DisplayAngle, FVector(0,0,0), arcVertices, arcIndices);

		if (!arcIndices.empty())
		{
			ID3D11Buffer* arcVB = nullptr;
			ID3D11Buffer* arcIB = nullptr;
			CreateTempBuffers(arcVertices, arcIndices, &arcVB, &arcIB);

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

		FVector Axis0 = bIsWorld ? LocalAxis0[AxisIndex] : GizmoRot.RotateVector(LocalAxis0[AxisIndex]);
		FVector Axis1 = bIsWorld ? LocalAxis1[AxisIndex] : GizmoRot.RotateVector(LocalAxis1[AxisIndex]);

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
		GenerateQuarterRingMesh(LocalRenderAxis0, LocalRenderAxis1,
			RotateCollisionConfig.InnerRadius, RotateCollisionConfig.OuterRadius, RotateCollisionConfig.Thickness,
			vertices, Indices);

		ID3D11Buffer* TempVB = nullptr;
		ID3D11Buffer* TempIB = nullptr;
		CreateTempBuffers(vertices, Indices, &TempVB, &TempIB);

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
