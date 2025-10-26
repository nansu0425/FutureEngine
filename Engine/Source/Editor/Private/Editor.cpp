#include "pch.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Axis.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Manager/UI/Public/UIManager.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"
#include "Global/Quaternion.h"
#include "Utility/Public/ScopeCycleCounter.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Component/Public/DecalSpotLightComponent.h"
#include "Component/Public/PointLightComponent.h"
#include "Physics/Public/BoundingSphere.h"
#include "Component/Public/DirectionalLightComponent.h"
#include "Component/Public/SpotLightComponent.h"
#include "Manager/UI/Public/ViewportManager.h"
#include "Render/ui/Viewport/Public/ViewportClient.h"
#include "Render/UI/Viewport/Public/Viewport.h"

IMPLEMENT_CLASS(UEditor, UObject)

UEditor::UEditor()
{
	int32 ActiveIndex = UViewportManager::GetInstance().GetActiveIndex();
	ActiveViewportIndex = ActiveIndex;
	Camera = UViewportManager::GetInstance().GetClients()[ActiveIndex]->GetCamera();
}

UEditor::~UEditor()
{
	// 레거시 코드 제거: ViewportManager가 뷰포트 관리
}

void UEditor::Update()
{
	UViewportManager& ViewportManager = UViewportManager::GetInstance();
	const UInputManager& Input = UInputManager::GetInstance();

	// 활성 카메라 업데이트
	int32 ActiveIndex = ViewportManager.GetActiveIndex();
	if (ActiveViewportIndex != ActiveIndex)
	{
		ActiveViewportIndex = ActiveIndex;
		Camera = ViewportManager.GetClients()[ActiveIndex]->GetCamera();
	}

	// KTLWeek07: 각 뷰포트에서 마우스 우클릭 시 해당 카메라만 입력 활성화
	int32 HoveredViewportIndex = ViewportManager.GetViewportIndexUnderMouse();
	bool bIsRightMouseDown = Input.IsKeyDown(EKeyInput::MouseRight);

	// 드래그 시작 시 뷰포트 고정, 드래그 종료 시 해제
	if (bIsRightMouseDown && !bWasRightMouseDown)
	{
		// 우클릭 시작: 현재 호버된 뷰포트를 잠금
		LockedViewportIndexForDrag = HoveredViewportIndex;
	}
	else if (!bIsRightMouseDown && bWasRightMouseDown)
	{
		// 우클릭 종료: 잠금 해제
		LockedViewportIndexForDrag = -1;
	}
	bWasRightMouseDown = bIsRightMouseDown;

	// 드래그 중이면 잠긴 뷰포트 사용, 아니면 호버된 뷰포트 사용
	int32 ActiveViewportIndexForInput = (LockedViewportIndexForDrag >= 0) ? LockedViewportIndexForDrag : HoveredViewportIndex;

	if (ViewportManager.GetViewportLayout() == EViewportLayout::Quad)
	{
		// Quad 모드: 마우스 우클릭 중이고 해당 뷰포트 위에 있을 때만 그 카메라 입력 활성화
		FViewportClient* ActiveOrthoClient = nullptr;

		for (int32 i = 0; i < 4; ++i)
		{
			if (ViewportManager.GetClients()[i] && ViewportManager.GetClients()[i]->GetCamera())
			{
				UCamera* Cam = ViewportManager.GetClients()[i]->GetCamera();
				// 마우스 우클릭 중이고 해당 뷰포트가 활성화된 뷰포트면 카메라 입력 활성화
				bool bEnableInput = (ActiveViewportIndexForInput == i && bIsRightMouseDown);
				Cam->SetInputEnabled(bEnableInput);

				// 오쏘 뷰가 활성화되었고 이동이 있었다면 기록
				if (bEnableInput && ViewportManager.GetClients()[i]->IsOrtho())
				{
					ActiveOrthoClient = ViewportManager.GetClients()[i];
				}
			}
		}

		// 오쏘 뷰 드래그 시 모든 오쏘 뷰를 공유 중심점 기준으로 업데이트
		if (ActiveOrthoClient && bIsRightMouseDown)
		{
			UCamera* ActiveOrthoCam = ActiveOrthoClient->GetCamera();
			if (ActiveOrthoCam)
			{
				// Camera::UpdateInput에서 이미 RelativeLocation이 업데이트됨
				// 공유 중심점 업데이트
				FVector CurrentLocation = ActiveOrthoCam->GetLocation();

				// ViewType에 따라 InitialOffsets 인덱스 결정
				int32 OrthoIdx = -1;
				switch (ActiveOrthoClient->GetViewType())
				{
				case EViewType::OrthoTop: OrthoIdx = 0; break;
				case EViewType::OrthoBottom: OrthoIdx = 1; break;
				case EViewType::OrthoLeft: OrthoIdx = 2; break;
				case EViewType::OrthoRight: OrthoIdx = 3; break;
				case EViewType::OrthoFront: OrthoIdx = 4; break;
				case EViewType::OrthoBack: OrthoIdx = 5; break;
				}

				if (OrthoIdx >= 0 && OrthoIdx < ViewportManager.GetInitialOffsets().size())
				{
					// 공유 중심점 = 현재 위치 - 초기 오프셋
					ViewportManager.SetOrthoGraphicCameraPoint(CurrentLocation - ViewportManager.GetInitialOffsets()[OrthoIdx]);

					// 모든 오쏘 뷰를 공유 중심점 기준으로 업데이트
					for (int32 i = 0; i < 4; ++i)
					{
						if (ViewportManager.GetClients()[i] && ViewportManager.GetClients()[i]->IsOrtho())
						{
							FViewportClient* Client = ViewportManager.GetClients()[i];
							int32 ClientOrthoIdx = -1;
							switch (Client->GetViewType())
							{
							case EViewType::OrthoTop: ClientOrthoIdx = 0; break;
							case EViewType::OrthoBottom: ClientOrthoIdx = 1; break;
							case EViewType::OrthoLeft: ClientOrthoIdx = 2; break;
							case EViewType::OrthoRight: ClientOrthoIdx = 3; break;
							case EViewType::OrthoFront: ClientOrthoIdx = 4; break;
							case EViewType::OrthoBack: ClientOrthoIdx = 5; break;
							}

							if (ClientOrthoIdx >= 0 && ClientOrthoIdx < ViewportManager.GetInitialOffsets().size() && Client->GetCamera())
							{
								FVector NewLocation = ViewportManager.GetOrthoGraphicCameraPoint() + ViewportManager.GetInitialOffsets()[ClientOrthoIdx];
								Client->GetCamera()->SetLocation(NewLocation);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// 싱글 모드: 뷰포트 위에서 마우스 우클릭 시 카메라 입력 활성화
		if (Camera)
		{
			// Single 모드에서는 ActiveViewportIndexForInput이 유효한 뷰포트면 입력 활성화
			bool bEnableInput = (ActiveViewportIndexForInput >= 0 && bIsRightMouseDown);
			Camera->SetInputEnabled(bEnableInput);
		}
	}

	UpdateBatchLines();
	BatchLines.UpdateVertexBuffer();

	UpdateCameraAnimation();

	ProcessMouseInput();
}

void UEditor::RenderEditor(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	BatchLines.Render();
	FAxis::Render(InCamera, InViewport);
}

void UEditor::RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	Gizmo.RenderGizmo(InCamera, InViewport);
	
	// 모든 DirectionalLight의 빛 방향 기즈모 렌더링 (선택 여부 무관)
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (EditorWorld && EditorWorld->GetLevel())
	{
		ULevel* CurrentLevel = EditorWorld->GetLevel();
		const TArray<ULightComponent*>& LightComponents = CurrentLevel->GetLightComponents();
		for (ULightComponent* LightComp : LightComponents)
		{
			if (UDirectionalLightComponent* DirLight = Cast<UDirectionalLightComponent>(LightComp))
			{
				DirLight->RenderLightDirectionGizmo(InCamera);
			}
			if (USpotLightComponent* DirLight = Cast<USpotLightComponent>(LightComp))
			{
				DirLight->RenderLightDirectionGizmo(InCamera);
			}
		}
	}
}




void UEditor::UpdateBatchLines()
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld || !EditorWorld->GetLevel()) { return; }

	uint64 ShowFlags = EditorWorld->GetLevel()->GetShowFlags();

	if (ShowFlags & EEngineShowFlags::SF_Octree)
	{
		BatchLines.UpdateOctreeVertices(EditorWorld->GetLevel()->GetStaticOctree());
	}
	else
	{
		// If we are not showing the octree, clear the lines, so they don't persist
		BatchLines.ClearOctreeLines();
	}

	if (UActorComponent* Component = GetSelectedComponent())
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
		{
			if (ShowFlags & EEngineShowFlags::SF_Bounds)
			{
				if (PrimitiveComponent->GetBoundingBox()->GetType() == EBoundingVolumeType::AABB)
				{
					FVector WorldMin, WorldMax; PrimitiveComponent->GetWorldAABB(WorldMin, WorldMax);
					FAABB AABB(WorldMin, WorldMax);
					BatchLines.UpdateBoundingBoxVertices(&AABB);
				}
				else 
				{ 
					BatchLines.UpdateBoundingBoxVertices(PrimitiveComponent->GetBoundingBox()); 

					// 만약 선택된 타입이 decalspotlightcomponent라면
					if (Component->IsA(UDecalSpotLightComponent::StaticClass()))
					{
						BatchLines.UpdateDecalSpotLightVertices(Cast<UDecalSpotLightComponent>(Component));
					}
				}
				return; 
			}
		}
		if (ULightComponent* LightComponent = Cast<ULightComponent>(Component))
		{
			if (ShowFlags & EEngineShowFlags::SF_Bounds)
			{
				if (UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Component))
				{
					if (USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Component))
					{
						const FVector Center = SpotLightComponent->GetWorldLocation();
						const float Radius = SpotLightComponent->GetAttenuationRadius();
						const float OuterRadian = SpotLightComponent->GetOuterConeAngle();
						const float InnerRadian = SpotLightComponent->GetInnerConeAngle();
						FQuaternion Rotation = SpotLightComponent->GetWorldRotationAsQuaternion();
						BatchLines.UpdateConeVertices(Center, Radius, OuterRadian, InnerRadian, Rotation);
						return;
					}
					const FVector Center = PointLightComponent->GetWorldLocation();
					const float Radius = PointLightComponent->GetAttenuationRadius();

					FBoundingSphere PointSphere(Center, Radius);
					BatchLines.UpdateBoundingBoxVertices(&PointSphere);
					return;
				}
				
			}
		}
	}

	BatchLines.DisableRenderBoundingBox();
}


void UEditor::ProcessMouseInput()
{
	// KTLWeek07: 활성 카메라 사용 (ViewportManager에서 관리)
	UCamera* CurrentCamera = Camera; // 생성자에서 설정된 활성 카메라
	if (!CurrentCamera)
	{
		return;
	}

	const UInputManager& InputManager = UInputManager::GetInstance();
	const FVector& MousePos = InputManager.GetMousePosition();

	// KTLWeek07: 활성 뷰포트의 정보 가져오기
	auto& ViewportManager = UViewportManager::GetInstance();
	int32 ActiveViewportIndex = ViewportManager.GetActiveIndex();
	FViewport* ActiveViewport = ViewportManager.GetViewports()[ActiveViewportIndex];
	if (!ActiveViewport) { return; }

	// PIE 뷰포트에서는 에디터 입력(오브젝트 선택, 기즈모 조작) 처리 안 함
	if (GEditor->IsPIESessionActive() && ActiveViewportIndex == ViewportManager.GetPIEActiveViewportIndex())
	{
		return;
	}

	const D3D11_VIEWPORT& ViewportInfo = ActiveViewport->GetRenderRect();

	AActor* ActorPicked = GetSelectedActor();
	if (ActorPicked)
	{
		// 피킹 전 현재 카메라에 맞는 기즈모 스케일 업데이트
		Gizmo.UpdateScale(CurrentCamera, ViewportInfo);
	}

	const float NdcX = ((MousePos.X - ViewportInfo.TopLeftX) / ViewportInfo.Width) * 2.0f - 1.0f;
	const float NdcY = -(((MousePos.Y - ViewportInfo.TopLeftY) / ViewportInfo.Height) * 2.0f - 1.0f);

	FRay WorldRay = CurrentCamera->ConvertToWorldRay(NdcX, NdcY);

	static EGizmoDirection PreviousGizmoDirection = EGizmoDirection::None;
	FVector CollisionPoint;
	float ActorDistance = -1;

	// 기즈모 World / Local 모드 전환
	if (InputManager.IsKeyDown(EKeyInput::Ctrl) && InputManager.IsKeyPressed(EKeyInput::Backtick))
	{
		Gizmo.IsWorldMode() ? Gizmo.SetLocal() : Gizmo.SetWorld();
	}
	if (InputManager.IsKeyPressed(EKeyInput::Space))
	{
		Gizmo.ChangeGizmoMode();
	}

	// W/E/R 키로 기즈모 모드 직접 전환 (우클릭 중이 아닐 때만)
	bool bIsRightMouseDown = InputManager.IsKeyDown(EKeyInput::MouseRight);
	if (!bIsRightMouseDown)
	{
		if (InputManager.IsKeyPressed(EKeyInput::W))
		{
			Gizmo.SetGizmoMode(EGizmoMode::Translate);
		}
		if (InputManager.IsKeyPressed(EKeyInput::E))
		{
			Gizmo.SetGizmoMode(EGizmoMode::Rotate);
		}
		if (InputManager.IsKeyPressed(EKeyInput::R))
		{
			Gizmo.SetGizmoMode(EGizmoMode::Scale);
		}
	}
	if (InputManager.IsKeyReleased(EKeyInput::MouseLeft))
	{
		Gizmo.EndDrag();
	}

	// 스플리터 드래그 여부 체크
	const bool bSplitterDragging = ViewportManager.IsAnySplitterDragging();

	if (Gizmo.IsDragging() && IsValid<USceneComponent>(Gizmo.GetSelectedComponent()))
	{
		switch (Gizmo.GetGizmoMode())
		{
		case EGizmoMode::Translate:
		{
			FVector GizmoDragLocation = GetGizmoDragLocation(CurrentCamera, WorldRay);
			Gizmo.SetLocation(GizmoDragLocation);
			break;
		}
		case EGizmoMode::Rotate:
		{
			FQuaternion GizmoDragRotation = GetGizmoDragRotation(CurrentCamera, WorldRay);
			Gizmo.SetComponentRotation(GizmoDragRotation);
			break;
		}
		case EGizmoMode::Scale:
		{
			FVector GizmoDragScale = GetGizmoDragScale(CurrentCamera, WorldRay);
			Gizmo.SetComponentScale(GizmoDragScale);
		}
		}
	}
	else if (!ImGui::GetIO().WantCaptureMouse && !bSplitterDragging)
	{
		// 스플리터 드래그 중이 아니고, ImGui가 마우스를 캡처하지 않았을 때만 처리
		if (GetSelectedActor() && Gizmo.HasComponent())
		{
			ObjectPicker.PickGizmo(CurrentCamera, WorldRay, Gizmo, CollisionPoint);
		}
		else
		{
			Gizmo.SetGizmoDirection(EGizmoDirection::None);
		}

		if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
		{
			// 뷰포트 클릭 시 LastClickedViewportIndex 업데이트 (PIE 시작 시 사용)
			ViewportManager.SetLastClickedViewportIndex(ActiveViewportIndex);

			UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
			if (EditorWorld && EditorWorld->GetLevel() && EditorWorld->GetLevel()->GetShowFlags())
			{
				TArray<UPrimitiveComponent*> Candidate;

				ULevel* CurrentLevel = EditorWorld->GetLevel();
				ObjectPicker.FindCandidateFromOctree(CurrentLevel->GetStaticOctree(), WorldRay, Candidate);

				TArray<UPrimitiveComponent*>& DynamicCandidates = CurrentLevel->GetDynamicPrimitives();
				if (!DynamicCandidates.empty())
				{
					Candidate.insert(Candidate.end(), DynamicCandidates.begin(), DynamicCandidates.end());
				}

				TStatId StatId("Picking");
				FScopeCycleCounter PickCounter(StatId);
				UPrimitiveComponent* PrimitiveCollided = ObjectPicker.PickPrimitive(CurrentCamera, WorldRay, Candidate, &ActorDistance);
				ActorPicked = PrimitiveCollided ? PrimitiveCollided->GetOwner() : nullptr;
				float ElapsedMs = static_cast<float>(PickCounter.Finish()); // 피킹 시간 측정 종료
				UStatOverlay::GetInstance().RecordPickingStats(ElapsedMs);
			}
		}

		if (Gizmo.GetGizmoDirection() == EGizmoDirection::None)
		{
			if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
			{
				SelectActor(ActorPicked);
			}
			if (PreviousGizmoDirection != EGizmoDirection::None)
			{
				Gizmo.OnMouseRelease(PreviousGizmoDirection);
			}
		}
		else
		{
			PreviousGizmoDirection = Gizmo.GetGizmoDirection();
			if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
			{
				Gizmo.OnMouseDragStart(CollisionPoint);
			}
			else
			{
				Gizmo.OnMouseHovering();
			}
		}
	}

	if (InputManager.IsKeyPressed(EKeyInput::F))
	{
		FocusOnSelectedActor();
	}
}

FVector UEditor::GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis);
	}

	// 카메라 방향 벡터를 사용하여 안정적인 평면 계산
	const FVector CamRight = InActiveCamera->GetRight();
	const FVector CamUp = InActiveCamera->GetUp();

	// GizmoAxis에 가장 수직인 카메라 벡터 선택
	FVector PlaneVector;
	const float DotRight = std::abs(GizmoAxis.Dot(CamRight));
	const float DotUp = std::abs(GizmoAxis.Dot(CamUp));

	if (DotRight < DotUp)
	{
		PlaneVector = CamRight;
	}
	else
	{
		PlaneVector = CamUp;
	}

	FVector PlaneNormal = GizmoAxis.Cross(PlaneVector);
	PlaneNormal.Normalize();

	// PlaneNormal이 카메라를 향하도록 보장 (드래그 방향 일관성)
	const FVector CameraLocation = InActiveCamera->GetLocation();
	const FVector ToCamera = (CameraLocation - PlaneOrigin).GetNormalized();
	if (PlaneNormal.Dot(ToCamera) < 0.0f)
	{
		PlaneNormal = -PlaneNormal;
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		FVector MouseDistance = MouseWorld - Gizmo.GetDragStartMouseLocation();
		return Gizmo.GetDragStartActorLocation() + GizmoAxis * MouseDistance.Dot(GizmoAxis);
	}
	return Gizmo.GetGizmoLocation();
}

FQuaternion UEditor::GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay)
{
	const FVector GizmoLocation = Gizmo.GetGizmoLocation();
	const FVector LocalGizmoAxis = Gizmo.GetGizmoAxis();
	const FQuaternion StartRotQuat = Gizmo.GetDragStartActorRotationQuat();

	// 월드 공간 회전축
	FVector WorldRotationAxis = LocalGizmoAxis;
	if (!Gizmo.IsWorldMode())
	{
		WorldRotationAxis = StartRotQuat.RotateVector(LocalGizmoAxis);
	}

	// 스크린 공간 회전 계산
	// 활성 뷰포트 정보 가져오기
	const FRect& ViewportRect = UViewportManager::GetInstance().GetActiveViewportRect();
	const float ViewportWidth = static_cast<float>(ViewportRect.Width);
	const float ViewportHeight = static_cast<float>(ViewportRect.Height);
	const float ViewportLeft = static_cast<float>(ViewportRect.Left);
	const float ViewportTop = static_cast<float>(ViewportRect.Top);

	// 기즈모 중심을 스크린 공간으로 투영
	const FCameraConstants& CamConst = InActiveCamera->GetFViewProjConstants();
	const FMatrix ViewProj = CamConst.View * CamConst.Projection;
	FVector4 GizmoScreenPos4 = FVector4(GizmoLocation, 1.0f) * ViewProj;

	if (GizmoScreenPos4.W > 0.0f)
	{
		// NDC로 변환
		GizmoScreenPos4 *= (1.0f / GizmoScreenPos4.W);

		// NDC → 뷰포트 로컬 좌표 (뷰포트 내 상대 좌표)
		const FVector2 GizmoLocalPos(
			(GizmoScreenPos4.X * 0.5f + 0.5f) * ViewportWidth,
			((-GizmoScreenPos4.Y) * 0.5f + 0.5f) * ViewportHeight
		);

		// 현재 마우스 스크린 좌표 (윈도우 전체 기준)
		POINT MousePos;
		GetCursorPos(&MousePos);
		ScreenToClient(GetActiveWindow(), &MousePos);

		// 마우스를 뷰포트 로컬 좌표로 변환
		const FVector2 CurrentScreenPos(
			static_cast<float>(MousePos.x) - ViewportLeft,
			static_cast<float>(MousePos.y) - ViewportTop
		);

		// 회전축의 스크린 공간 방향 계산
		FVector2 TangentDir;

		if (InActiveCamera->GetCameraType() == ECameraType::ECT_Orthographic)
		{
			// 직교 뷰: 회전 평면을 이루는 두 축의 끝점을 스크린에 투영
			const FVector Axis0 = Gizmo.GetGizmoDirection() == EGizmoDirection::Forward ? FVector(0, 0, 1) :
			                      Gizmo.GetGizmoDirection() == EGizmoDirection::Right ? FVector(0, 0, 1) :
			                      FVector(1, 0, 0);
			const FVector Axis1 = Gizmo.GetGizmoDirection() == EGizmoDirection::Forward ? FVector(0, 1, 0) :
			                      Gizmo.GetGizmoDirection() == EGizmoDirection::Right ? FVector(1, 0, 0) :
			                      FVector(0, 1, 0);

			const FQuaternion RotQuat = Gizmo.IsWorldMode() ? FQuaternion::Identity() : Gizmo.GetDragStartActorRotationQuat();
			const FVector RotatedAxis0 = RotQuat.RotateVector(Axis0);
			const FVector RotatedAxis1 = RotQuat.RotateVector(Axis1);

			FVector4 Axis0World4 = FVector4(GizmoLocation + RotatedAxis0 * 64.0f, 1.0f);
			FVector4 Axis1World4 = FVector4(GizmoLocation + RotatedAxis1 * 64.0f, 1.0f);
			FVector4 Axis0Screen4 = Axis0World4 * ViewProj;
			FVector4 Axis1Screen4 = Axis1World4 * ViewProj;

			FVector2 Axis0ScreenPos(0, 0);
			FVector2 Axis1ScreenPos(0, 0);

			if (Axis0Screen4.W > 0.0f)
			{
				Axis0Screen4 *= (1.0f / Axis0Screen4.W);
				Axis0ScreenPos = FVector2(
					(Axis0Screen4.X * 0.5f + 0.5f) * ViewportWidth,
					((-Axis0Screen4.Y) * 0.5f + 0.5f) * ViewportHeight
				);
			}

			if (Axis1Screen4.W > 0.0f)
			{
				Axis1Screen4 *= (1.0f / Axis1Screen4.W);
				Axis1ScreenPos = FVector2(
					(Axis1Screen4.X * 0.5f + 0.5f) * ViewportWidth,
					((-Axis1Screen4.Y) * 0.5f + 0.5f) * ViewportHeight
				);
			}

			FVector2 AxisScreenDir = (Axis1ScreenPos - Axis0ScreenPos).GetNormalized();
			TangentDir = FVector2(-AxisScreenDir.Y, AxisScreenDir.X);
		}
		else
		{
			// 원근 뷰: 축 방향을 View 변환하여 스크린 방향 계산
			FVector4 AxisDirView = FVector4(WorldRotationAxis, 0.0f) * CamConst.View;
			FVector2 AxisScreenDir(AxisDirView.X, -AxisDirView.Y);
			AxisScreenDir = AxisScreenDir.GetNormalized();
			TangentDir = FVector2(-AxisScreenDir.Y, AxisScreenDir.X);
		}

		// 스크린 공간 드래그 벡터 (정규화하지 않음)
		const FVector2 PrevScreenPos = Gizmo.GetPreviousScreenPos();
		const FVector2 DragDir = CurrentScreenPos - PrevScreenPos;

		// 마우스가 실제로 움직였는지 체크
		const float DragDistSq = DragDir.LengthSquared();
		constexpr float MinDragDistSq = 0.1f * 0.1f;

		if (DragDistSq > MinDragDistSq)
		{
			// Dot Product로 회전 각도 계산
			float PixelDelta = TangentDir.Dot(DragDir);

			// 픽셀을 각도(라디안)로 변환 (언리얼에선 1픽셀 = 1도 사용, 어차피 감도 조절용 매직 넘버라 그대로 차용)
			constexpr float PixelsToDegrees = 1.0f;
			float DeltaAngleDegrees = PixelDelta * PixelsToDegrees;
			float DeltaAngle = FVector::GetDegreeToRadian(DeltaAngleDegrees);

			// 누적 각도 업데이트
			float NewAngle = Gizmo.GetCurrentRotationAngle() + DeltaAngle;

			// 360deg Clamp
			constexpr float TwoPi = 2.0f * PI;
			if (NewAngle > TwoPi)
			{
				NewAngle = fmodf(NewAngle, TwoPi);
			}
			else if (NewAngle < -TwoPi)
			{
				NewAngle = fmodf(NewAngle, -TwoPi);
			}

			Gizmo.SetCurrentRotationAngle(NewAngle);
		}

		// 현재 스크린 좌표 저장
		Gizmo.SetPreviousScreenPos(CurrentScreenPos);

		// 최종 회전 Quaternion 계산 (스냅 적용)
		float FinalAngle = Gizmo.GetCurrentRotationAngle();
		if (UViewportManager::GetInstance().IsRotationSnapEnabled())
		{
			const float SnapAngleDegrees = UViewportManager::GetInstance().GetRotationSnapAngle();
			const float SnapAngleRadians = FVector::GetDegreeToRadian(SnapAngleDegrees);
			FinalAngle = std::round(Gizmo.GetCurrentRotationAngle() / SnapAngleRadians) * SnapAngleRadians;
		}

		const FQuaternion DeltaRotQuat = FQuaternion::FromAxisAngle(LocalGizmoAxis, FinalAngle);
		if (Gizmo.IsWorldMode())
		{
			return StartRotQuat * DeltaRotQuat;
		}
		else
		{
			return DeltaRotQuat * StartRotQuat;
		}
	}

	return Gizmo.GetComponentRotation();
}

FVector UEditor::GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin = Gizmo.GetGizmoLocation();
	FVector CardinalAxis = Gizmo.GetGizmoAxis();

	// Scale 모드는 항상 로컬 좌표를 사용하므로 GizmoAxis를 월드 좌표로 변환
	FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
	FVector GizmoAxis = q.RotateVector(CardinalAxis);

	// 카메라 방향 벡터를 사용하여 안정적인 평면 계산
	const FVector CamForward = InActiveCamera->GetForward();
	const FVector CamRight = InActiveCamera->GetRight();
	const FVector CamUp = InActiveCamera->GetUp();

	// GizmoAxis에 가장 수직인 카메라 벡터 선택
	FVector PlaneVector;
	const float DotRight = std::abs(GizmoAxis.Dot(CamRight));
	const float DotUp = std::abs(GizmoAxis.Dot(CamUp));

	if (DotRight < DotUp)
	{
		PlaneVector = CamRight;
	}
	else
	{
		PlaneVector = CamUp;
	}

	FVector PlaneNormal = GizmoAxis.Cross(PlaneVector);
	PlaneNormal.Normalize();

	// PlaneNormal이 카메라를 향하도록 보장 (드래그 방향 일관성)
	const FVector CameraLocation = InActiveCamera->GetLocation();
	const FVector ToCamera = (CameraLocation - PlaneOrigin).GetNormalized();
	if (PlaneNormal.Dot(ToCamera) < 0.0f)
	{
		PlaneNormal = -PlaneNormal;
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		// 언리얼 방식: 드래그 거리에 비례하여 스케일 변화
		const FVector MouseDelta = MouseWorld - Gizmo.GetDragStartMouseLocation();
		const float AxisDragDistance = MouseDelta.Dot(GizmoAxis);

		// 카메라 거리에 따른 스케일 민감도 조정
		const FVector CameraLocation = InActiveCamera->GetLocation();
		const float DistanceToGizmo = (PlaneOrigin - CameraLocation).Length();

		// 언리얼 기준: 거리에 비례한 민감도, 기본 배율 적용
		// 가까울수록 정밀하게, 멀수록 빠르게 조정
		const float BaseSensitivity = 0.03f;  // 기본 민감도
		const float ScaleSensitivity = BaseSensitivity * DistanceToGizmo;
		const float ScaleDelta = AxisDragDistance * ScaleSensitivity;

		const FVector DragStartScale = Gizmo.GetDragStartActorScale();
		FVector NewScale;

		if (Gizmo.GetSelectedComponent()->IsUniformScale())
		{
			// Uniform Scale: 모든 축에 동일한 스케일 델타 적용
			const float UniformDelta = ScaleDelta;
			NewScale = DragStartScale + FVector(UniformDelta, UniformDelta, UniformDelta);

			// 모든 축이 최소값 이상이 되도록 보장
			NewScale.X = std::max(NewScale.X, MIN_SCALE_VALUE);
			NewScale.Y = std::max(NewScale.Y, MIN_SCALE_VALUE);
			NewScale.Z = std::max(NewScale.Z, MIN_SCALE_VALUE);
		}
		else
		{
			// Non-uniform Scale: 선택한 축에만 스케일 델타 적용
			NewScale = DragStartScale;

			// X축 (1,0,0)
			if (std::abs(CardinalAxis.X) > 0.5f)
			{
				NewScale.X = std::max(DragStartScale.X + ScaleDelta, MIN_SCALE_VALUE);
			}
			// Y축 (0,1,0)
			else if (std::abs(CardinalAxis.Y) > 0.5f)
			{
				NewScale.Y = std::max(DragStartScale.Y + ScaleDelta, MIN_SCALE_VALUE);
			}
			// Z축 (0,0,1)
			else if (std::abs(CardinalAxis.Z) > 0.5f)
			{
				NewScale.Z = std::max(DragStartScale.Z + ScaleDelta, MIN_SCALE_VALUE);
			}
		}

		return NewScale;
	}
	return Gizmo.GetComponentScale();
}

void UEditor::SelectActor(AActor* InActor)
{
	if (InActor == SelectedActor) return;

	SelectedActor = InActor;
	if (SelectedActor) { SelectComponent(InActor->GetRootComponent()); }
	else { SelectComponent(nullptr); }
}

void UEditor::SelectComponent(UActorComponent* InComponent)
{
	if (InComponent == SelectedComponent) return;
	
	if (SelectedComponent)
	{
		SelectedComponent->OnDeselected();
	}

	SelectedComponent = InComponent;
	if (SelectedComponent)
	{
		SelectedComponent->OnSelected();
	}
	UUIManager::GetInstance().OnSelectedComponentChanged(SelectedComponent);
}

void UEditor::FocusOnSelectedActor()
{
	if (!SelectedActor)
	{
		return;
	}

	auto& ViewportManager = UViewportManager::GetInstance();
	auto& Viewports = ViewportManager.GetViewports();
	auto& Clients = ViewportManager.GetClients();

	if (Viewports.empty() || Clients.empty()) { return; }

	UPrimitiveComponent* Prim = nullptr;
	if (SelectedActor->GetRootComponent() && SelectedActor->GetRootComponent()->IsA(UPrimitiveComponent::StaticClass()))
	{
		Prim = Cast<UPrimitiveComponent>(SelectedActor->GetRootComponent());
	}

	if (!Prim)
	{
		return;
	}

	FVector ComponentMin, ComponentMax;
	Prim->GetWorldAABB(ComponentMin, ComponentMax);
	const FVector Center = (ComponentMin + ComponentMax) * 0.5f;
	const FVector AABBSize = ComponentMax - ComponentMin;
	const float BoundingRadius = AABBSize.Length() * 0.5f;

	const int32 ViewportCount = static_cast<int32>(Viewports.size());

	CameraStartLocation.resize(ViewportCount);
	CameraStartRotation.resize(ViewportCount);
	CameraTargetLocation.resize(ViewportCount);
	CameraTargetRotation.resize(ViewportCount);

	// 활성 뷰포트의 카메라 타입 가져오기
	const int32 ActiveIdx = ViewportManager.GetActiveIndex();
	if (ActiveIdx < 0 || ActiveIdx >= ViewportCount || !Clients[ActiveIdx])
	{
		return;
	}

	UCamera* ActiveCam = Clients[ActiveIdx]->GetCamera();
	if (!ActiveCam)
	{
		return;
	}

	const ECameraType ActiveCameraType = ActiveCam->GetCameraType();
	AnimatingCameraType = ActiveCameraType;

	for (int32 i = 0; i < ViewportCount; ++i)
	{
		if (!Clients[i]) continue;
		UCamera* Cam = Clients[i]->GetCamera();
		if (!Cam) continue;

		// 모든 카메라의 현재 상태 저장 (애니메이션 필터링은 나중에)
		CameraStartLocation[i] = Cam->GetLocation();
		CameraStartRotation[i] = Cam->GetRotation();

		// 활성 뷰포트와 동일한 카메라 타입만 목표 위치 계산
		if (Cam->GetCameraType() != ActiveCameraType)
		{
			// 타입이 다르면 현재 위치를 목표로 (애니메이션 안 함)
			CameraTargetLocation[i] = CameraStartLocation[i];
			CameraTargetRotation[i] = CameraStartRotation[i];
			continue;
		}

		if (Cam->GetCameraType() == ECameraType::ECT_Perspective)
		{
			// 카메라의 Forward 벡터를 직접 사용 (오일러각 대신)
			const FVector Forward = Cam->GetForward();

			const float FovY = Cam->GetFovY();
			const float HalfFovRadian = FVector::GetDegreeToRadian(FovY * 0.5f);

			// AABB 크기만큼 추가 간격 확보
			const float AABBMargin = max(AABBSize.X, max(AABBSize.Y, AABBSize.Z));
			const float Distance = (BoundingRadius / sinf(HalfFovRadian)) + AABBMargin;

			CameraTargetLocation[i] = Center - Forward * Distance;

			// 목표 회전은 현재 회전 유지 (카메라 각도가 바뀌지 않음)
			CameraTargetRotation[i] = Cam->GetRotation();
		}
		else
		{
			// Orthographic: Center를 화면 중앙에 오도록 카메라 위치 조정
			const FVector CurrentLocation = Cam->GetLocation();
			const FVector CurrentRotation = Cam->GetRotation();

			// 카메라 기저 벡터 (Forward, Right, Up)
			const FVector Forward = Cam->GetForward();
			const FVector Right = Cam->GetRight();
			const FVector Up = Cam->GetUp();

			// 현재 카메라에서 Center까지의 벡터
			const FVector ToCenter = Center - CurrentLocation;

			// Center가 카메라 좌표계에서 얼마나 떨어져 있는지 계산
			const float ForwardDist = ToCenter.Dot(Forward);  // Forward 방향 거리
			const float RightDist = ToCenter.Dot(Right);      // Right 방향 오프셋
			const float UpDist = ToCenter.Dot(Up);            // Up 방향 오프셋

			// 카메라를 Right/Up 방향으로 이동시켜 Center를 화면 중앙에 배치
			// Forward 거리는 적당히 유지 (100 단위)
			CameraTargetLocation[i] = CurrentLocation + Right * RightDist + Up * UpDist;

			// 회전은 현재 유지
			CameraTargetRotation[i] = CurrentRotation;
		}
	}

	bIsCameraAnimating = true;
	CameraAnimationTime = 0.0f;
}

void UEditor::UpdateCameraAnimation()
{
	if (!bIsCameraAnimating)
	{
		return;
	}

	auto& ViewportManager = UViewportManager::GetInstance();
	auto& Clients = ViewportManager.GetClients();
	const UInputManager& Input = UInputManager::GetInstance();

	if (Clients.empty())
	{
		bIsCameraAnimating = false;
		return;
	}

	// 우클릭 드래그 시작 시 애니메이션 중단
	if (Input.IsKeyPressed(EKeyInput::MouseRight))
	{
		bIsCameraAnimating = false;
		return;
	}

	CameraAnimationTime += DT;
	float Progress = CameraAnimationTime / CAMERA_ANIMATION_DURATION;

	if (Progress >= 1.0f)
	{
		Progress = 1.0f;
		bIsCameraAnimating = false;
	}

	float SmoothProgress;
	if (Progress < 0.5f)
	{
		SmoothProgress = 8.0f * Progress * Progress * Progress * Progress;
	}
	else
	{
		float ProgressFromEnd = Progress - 1.0f;
		SmoothProgress = 1.0f - 8.0f * ProgressFromEnd * ProgressFromEnd * ProgressFromEnd * ProgressFromEnd;
	}

	const size_t AnimationVectorSize = CameraStartLocation.size();
	for (int Index = 0; Index < Clients.size() && Index < AnimationVectorSize; ++Index)
	{
		if (!Clients[Index]) continue;
		UCamera* Cam = Clients[Index]->GetCamera();
		if (!Cam) continue;

		// 애니메이션 시작 시 결정된 카메라 타입과 일치하는 경우만 처리
		if (Cam->GetCameraType() != AnimatingCameraType)
		{
			continue;
		}

		FVector CurrentLocation = CameraStartLocation[Index] + (CameraTargetLocation[Index] - CameraStartLocation[Index]) * SmoothProgress;
		Cam->SetLocation(CurrentLocation);

		if (Cam->GetCameraType() == ECameraType::ECT_Perspective)
		{
			FVector CurrentRotation = CameraStartRotation[Index] + (CameraTargetRotation[Index] - CameraStartRotation[Index]) * SmoothProgress;
			Cam->SetRotation(CurrentRotation);
		}
		else
		{
			// Orthographic: 회전도 보간 (ViewType 전환 등을 위해)
			FVector CurrentRotation = CameraStartRotation[Index] + (CameraTargetRotation[Index] - CameraStartRotation[Index]) * SmoothProgress;
			Cam->SetRotation(CurrentRotation);
		}
	}
}
