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
	if (GEditor->IsPIESessionActive())
	{
		return;
	}
	BatchLines.Render();
	FAxis::Render(InCamera, InViewport);
}

void UEditor::RenderGizmo(UCamera* InCamera, const D3D11_VIEWPORT& InViewport)
{
	if (GEditor->IsPIESessionActive())
	{
		return;
	}
	Gizmo.RenderGizmo(InCamera, InViewport);
	
	// 모든 DirectionalLight의 빛 방향 기즈모 렌더링 (선택 여부 무관)
	if (ULevel* CurrentLevel = GWorld->GetLevel())
	{
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
	uint64 ShowFlags = GWorld->GetLevel()->GetShowFlags();

	if (ShowFlags & EEngineShowFlags::SF_Octree)
	{
		BatchLines.UpdateOctreeVertices(GWorld->GetLevel()->GetStaticOctree());
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
	FViewport* ActiveViewport = ViewportManager.GetViewports()[ViewportManager.GetActiveIndex()];
	if (!ActiveViewport) { return; }

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
	else
	{
		if (GetSelectedActor() && Gizmo.HasComponent())
		{
			ObjectPicker.PickGizmo(CurrentCamera, WorldRay, Gizmo, CollisionPoint);
		}
		else
		{
			Gizmo.SetGizmoDirection(EGizmoDirection::None);
		}

		if (!ImGui::GetIO().WantCaptureMouse && InputManager.IsKeyPressed(EKeyInput::MouseLeft))
		{
			if (GWorld->GetLevel()->GetShowFlags())
			{
				TArray<UPrimitiveComponent*> Candidate;

				ULevel* CurrentLevel = GWorld->GetLevel();
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
			SelectActor(ActorPicked);
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
		// RotationMatrix 로직에 문제
		// FVector4 GizmoAxis4{ GizmoAxis.X, GizmoAxis.Y, GizmoAxis.Z, 0.0f };
		// FVector RadRotation = FVector::GetDegreeToRadian(Gizmo.GetComponentRotation());
		// GizmoAxis = GizmoAxis4 * FMatrix::RotationMatrix(RadRotation);
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis); 
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, GizmoAxis.Cross(InActiveCamera->CalculatePlaneNormal(GizmoAxis)), MouseWorld))
	{
		FVector MouseDistance = MouseWorld - Gizmo.GetDragStartMouseLocation();
		return Gizmo.GetDragStartActorLocation() + GizmoAxis * MouseDistance.Dot(GizmoAxis);
	}
	return Gizmo.GetGizmoLocation();
}

FQuaternion UEditor::GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis);
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, GizmoAxis, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		PlaneOriginToMouse.Normalize();
		PlaneOriginToMouseStart.Normalize();
		float DotResult = (PlaneOriginToMouseStart).Dot(PlaneOriginToMouse);
		float Angle = acosf(std::max(-1.0f, std::min(1.0f, DotResult)));
		if ((PlaneOriginToMouse.Cross(PlaneOriginToMouseStart)).Dot(GizmoAxis) < 0)
		{
			Angle = -Angle;
		}

		FQuaternion StartRotQuat = Gizmo.GetDragStartActorRotationQuat();
		// 로컬 모드일 때는 이미 월드 좌표로 변환된 GizmoAxis 사용
		FQuaternion DeltaRotQuat = FQuaternion::FromAxisAngle(GizmoAxis, Angle);
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

	// 로컬 모드에서는 GizmoAxis를 월드 좌표로 변환
	FVector GizmoAxis = CardinalAxis;
	if (!Gizmo.IsWorldMode())
	{
		FQuaternion q = Gizmo.GetTargetComponent()->GetWorldRotationAsQuaternion();
		GizmoAxis = q.RotateVector(GizmoAxis);
	}

	FVector PlaneNormal = GizmoAxis.Cross(InActiveCamera->CalculatePlaneNormal(GizmoAxis));
	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		float DragStartAxisDistance = PlaneOriginToMouseStart.Dot(GizmoAxis);
		float DragAxisDistance = PlaneOriginToMouse.Dot(GizmoAxis);
		float ScaleFactor = 1.0f;
		if (std::fabsf(DragStartAxisDistance) > 0.1f)
		{
			ScaleFactor = DragAxisDistance / DragStartAxisDistance;
		}

		FVector DragStartScale = Gizmo.GetDragStartActorScale();
		if (ScaleFactor > MinScale)
		{
			if (Gizmo.GetSelectedComponent()->IsUniformScale())
			{
				float UniformValue = DragStartScale.Dot(CardinalAxis);
				return FVector(1.0f, 1.0f, 1.0f) * UniformValue * ScaleFactor;
			}
			else
			{
				return DragStartScale + CardinalAxis * (ScaleFactor - 1.0f) * DragStartScale.Dot(CardinalAxis);
			}
		}
		return Gizmo.GetComponentScale();
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

	for (int32 i = 0; i < ViewportCount; ++i)
	{
		if (!Clients[i]) continue;
		UCamera* Cam = Clients[i]->GetCamera();
		if (!Cam) continue;

		CameraStartLocation[i] = Cam->GetLocation();
		CameraStartRotation[i] = Cam->GetRotation();

		if (Cam->GetCameraType() == ECameraType::ECT_Perspective)
		{
			// 현재 카메라 각도에서 Forward 벡터 계산
			const FVector CurrentRotation = Cam->GetRotation();
			const float Yaw = FVector::GetDegreeToRadian(CurrentRotation.Z);
			const float Pitch = FVector::GetDegreeToRadian(CurrentRotation.Y);
			const FVector Forward(
				cosf(Pitch) * cosf(Yaw),
				cosf(Pitch) * sinf(Yaw),
				sinf(Pitch)
			);

			const float FovY = Cam->GetFovY();
			const float HalfFovRadian = FVector::GetDegreeToRadian(FovY * 0.5f);

			// AABB 크기만큼 추가 간격 확보
			const float AABBMargin = max(AABBSize.X, max(AABBSize.Y, AABBSize.Z));
			const float Distance = (BoundingRadius / sinf(HalfFovRadian)) + AABBMargin;

			CameraTargetLocation[i] = Center - Forward * Distance;

			// 목표 회전은 현재 회전 유지 (카메라 각도가 바뀌지 않음)
			CameraTargetRotation[i] = CurrentRotation;
		}
		else
		{
			// Orthographic: 현재 뷰 방향 유지하면서 위치만 조정
			const FVector CurrentLocation = Cam->GetLocation();
			const FVector CurrentRotation = Cam->GetRotation();

			// Forward 벡터 계산 (오쏘는 고정 방향이지만 일관성을 위해)
			const FVector Forward = Cam->GetForward();

			// Actor 위치에서 현재 위치로의 벡터
			const FVector ToCamera = CurrentLocation - Center;

			// Forward 방향의 거리는 유지
			const float ForwardDistance = ToCamera.Dot(Forward);

			// Center에서 Forward 방향으로 현재 거리만큼 떨어진 지점
			CameraTargetLocation[i] = Center + Forward * ForwardDistance;
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

		FVector CurrentLocation = CameraStartLocation[Index] + (CameraTargetLocation[Index] - CameraStartLocation[Index]) * SmoothProgress;
		Cam->SetLocation(CurrentLocation);

		if (Cam->GetCameraType() == ECameraType::ECT_Perspective)
		{
			FVector CurrentRotation = CameraStartRotation[Index] + (CameraTargetRotation[Index] - CameraStartRotation[Index]) * SmoothProgress;
			Cam->SetRotation(CurrentRotation);
		}
	}
}
