#include "pch.h"

#include "Render/UI/Viewport/Public/Viewport.h"
#include "Render/UI/Viewport/Public/ViewportClient.h"
#include "Manager/UI/Public/ViewportManager.h"

FViewportClient::FViewportClient()
{
    ViewportCamera = NewObject<UCamera>();

if (ViewportCamera)
    {
        ViewportCamera->SetMoveSpeed(UViewportManager::GetInstance().GetEditorCameraSpeed());
    }
}

FViewportClient::~FViewportClient()
{
    SafeDelete(ViewportCamera);
}

void FViewportClient::SetViewType(EViewType InType)
{
    ViewType = InType;
    
    if (!ViewportCamera) return;
    
    // Set camera type and orientation based on view type
    switch (InType)
    {
    case EViewType::Perspective:
        ViewportCamera->SetCameraType(ECameraType::ECT_Perspective);
        // Restore saved perspective location, rotation and far clip
        ViewportCamera->SetLocation(SavedPerspectiveLocation);
        ViewportCamera->SetRotation(SavedPerspectiveRotation);
        ViewportCamera->SetFarZ(SavedPerspectiveFarZ);
        break;
        
    case EViewType::OrthoTop:
        // Save current state if coming from perspective
        if (ViewportCamera->GetCameraType() == ECameraType::ECT_Perspective)
        {
            SavedPerspectiveLocation = ViewportCamera->GetLocation();
            SavedPerspectiveRotation = ViewportCamera->GetRotation();
            SavedPerspectiveFarZ = ViewportCamera->GetFarZ();
        }

        ViewportCamera->SetCameraType(ECameraType::ECT_Orthographic);
        ViewportCamera->SetFarZ(5000.0f);
        ViewportCamera->SetOrthoWidth(50.0f);
        // Top view: Looking down (-Z direction)
        ViewportCamera->SetRotation(FVector(0.0f, 0.0f, 0.0f));
        ViewportCamera->SetLocation(FVector(0.0f, 0.0f, 100.0f));
        ViewportCamera->SetForward(FVector(0.0f, 0.0f, -1.0f));
        ViewportCamera->SetRight(FVector(0.0f, -1.0f, 0.0f));
        ViewportCamera->SetUp(FVector(-1.0f, 0.0f, 0.0f));
        break;
        
    case EViewType::OrthoBottom:
        if (ViewportCamera->GetCameraType() == ECameraType::ECT_Perspective)
        {
            SavedPerspectiveLocation = ViewportCamera->GetLocation();
            SavedPerspectiveRotation = ViewportCamera->GetRotation();
            SavedPerspectiveFarZ = ViewportCamera->GetFarZ();
        }

        ViewportCamera->SetCameraType(ECameraType::ECT_Orthographic);
        ViewportCamera->SetFarZ(5000.0f);
        ViewportCamera->SetOrthoWidth(50.0f);
        // Bottom view: Looking up (+Z direction) - X가 아래
        ViewportCamera->SetRotation(FVector(0.0f, -90.0f, 0.0f));
        ViewportCamera->SetLocation(FVector(0.0f, 0.0f, -50.0f));
        ViewportCamera->SetForward(FVector(0.0f, 0.0f, 1.0f));
        ViewportCamera->SetRight(FVector(0.0f, 1.0f, 0.0f));
        ViewportCamera->SetUp(FVector(-1.0f, 0.0f, 0.0f));
        break;
        
    case EViewType::OrthoFront:
        if (ViewportCamera->GetCameraType() == ECameraType::ECT_Perspective)
        {
            SavedPerspectiveLocation = ViewportCamera->GetLocation();
            SavedPerspectiveRotation = ViewportCamera->GetRotation();
            SavedPerspectiveFarZ = ViewportCamera->GetFarZ();
        }

        ViewportCamera->SetCameraType(ECameraType::ECT_Orthographic);
        ViewportCamera->SetFarZ(5000.0f);
        ViewportCamera->SetOrthoWidth(50.0f);
        // Front view: Looking backward (-X direction) - Y가 왼쪽
        ViewportCamera->SetRotation(FVector(0.0f, 0.0f, 180.0f));
        ViewportCamera->SetLocation(FVector(100.0f, 0.0f, 0.0f));
        ViewportCamera->SetForward(FVector(-1.0f, 0.0f, 0.0f));
        ViewportCamera->SetRight(FVector(0.0f, -1.0f, 0.0f));
        ViewportCamera->SetUp(FVector(0.0f, 0.0f, 1.0f));
        break;

    case EViewType::OrthoBack:
        if (ViewportCamera->GetCameraType() == ECameraType::ECT_Perspective)
        {
            SavedPerspectiveLocation = ViewportCamera->GetLocation();
            SavedPerspectiveRotation = ViewportCamera->GetRotation();
            SavedPerspectiveFarZ = ViewportCamera->GetFarZ();
        }

        ViewportCamera->SetCameraType(ECameraType::ECT_Orthographic);
        ViewportCamera->SetFarZ(5000.0f);
        ViewportCamera->SetOrthoWidth(50.0f);
        // Back view: Looking forward (+X direction) - Y가 오른쪽
        ViewportCamera->SetRotation(FVector(0.0f, 0.0f, 0.0f));
        ViewportCamera->SetLocation(FVector(-100.0f, 0.0f, 0.0f));
        ViewportCamera->SetForward(FVector(1.0f, 0.0f, 0.0f));
        ViewportCamera->SetRight(FVector(0.0f, 1.0f, 0.0f));
        ViewportCamera->SetUp(FVector(0.0f, 0.0f, 1.0f));
        break;
        
    case EViewType::OrthoRight:
        if (ViewportCamera->GetCameraType() == ECameraType::ECT_Perspective)
        {
            SavedPerspectiveLocation = ViewportCamera->GetLocation();
            SavedPerspectiveRotation = ViewportCamera->GetRotation();
            SavedPerspectiveFarZ = ViewportCamera->GetFarZ();
        }

        ViewportCamera->SetCameraType(ECameraType::ECT_Orthographic);
        ViewportCamera->SetFarZ(5000.0f);
        ViewportCamera->SetOrthoWidth(50.0f);
        // Right view: Looking right (+Y direction)
        ViewportCamera->SetRotation(FVector(0.0f, 0.0f, 0.0f));
        ViewportCamera->SetLocation(FVector(0.0f, -100.0f, 0.0f));
        ViewportCamera->SetForward(FVector(0.0f, 1.0f, 0.0f));
        ViewportCamera->SetRight(FVector(-1.0f, 0.0f, 0.0f));
        ViewportCamera->SetUp(FVector(0.0f, 0.0f, 1.0f));
        break;
        
    case EViewType::OrthoLeft:
        if (ViewportCamera->GetCameraType() == ECameraType::ECT_Perspective)
        {
            SavedPerspectiveLocation = ViewportCamera->GetLocation();
            SavedPerspectiveRotation = ViewportCamera->GetRotation();
            SavedPerspectiveFarZ = ViewportCamera->GetFarZ();
        }

        ViewportCamera->SetCameraType(ECameraType::ECT_Orthographic);
        ViewportCamera->SetFarZ(5000.0f);
        ViewportCamera->SetOrthoWidth(50.0f);
        // Left view: Looking left (-Y direction)
        ViewportCamera->SetRotation(FVector(0.0f, 0.0f, -90.0f));
        ViewportCamera->SetLocation(FVector(0.0f, 50.0f, 0.0f));
        ViewportCamera->SetForward(FVector(0.0f, -1.0f, 0.0f));
        ViewportCamera->SetRight(FVector(1.0f, 0.0f, 0.0f));
        ViewportCamera->SetUp(FVector(0.0f, 0.0f, 1.0f));
        break;
    }
}

void FViewportClient::Tick() const
{
    // FutureEngine 철학: Camera는 Viewport의 크기 정보를 받아야 함
    if (OwningViewport && ViewportCamera)
    {
        const D3D11_VIEWPORT ViewportRect = OwningViewport->GetRenderRect();
        ViewportCamera->Update(ViewportRect);
    }
}

void FViewportClient::Draw(const FViewport* InViewport) const
{
    if (!InViewport) { return; }

    const float Aspect = InViewport->GetAspect();
    ViewportCamera->SetAspect(Aspect);

    if (IsOrtho())
    {
        ViewportCamera->UpdateMatrixByOrth();
    }
    else
    {
        ViewportCamera->UpdateMatrixByPers();
    }
}
