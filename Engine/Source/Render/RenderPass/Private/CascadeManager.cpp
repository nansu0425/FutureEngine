#include "pch.h"
#include "Render/RenderPass/Public/CascadeManager.h"

IMPLEMENT_SINGLETON_CLASS(UCascadeManager, UObject)

UCascadeManager::UCascadeManager() = default;
UCascadeManager::~UCascadeManager() = default;

uint32 UCascadeManager::GetSplitNum() const
{
    return SplitNum;
}

void UCascadeManager::SetSplitNum(uint32 InSplitNum)
{
    SplitNum = InSplitNum;
}

float UCascadeManager::GetSplitBlendFactor() const
{
    return SplitBlendFactor;
}

void UCascadeManager::SetSplitBlendFactor(float InSplitBlendFactor)
{
    SplitBlendFactor = InSplitBlendFactor;
}

float UCascadeManager::GetLightViewVolumeZNearBias() const
{
    return LightViewVolumeZNearBias;
}

void UCascadeManager::SetLightViewVolumeZNearBias(float InLightViewVolumeZNearBias)
{
    LightViewVolumeZNearBias = InLightViewVolumeZNearBias;
}

float UCascadeManager::CalculateFrustumXYWithZ(float Z, float Fov)
{
    return tan(Fov / 2.0f) * Z;
}

FCascadeShadowMapData UCascadeManager::GetCascadeShadowMapData(
    UCamera* InCamera,
    UDirectionalLightComponent* InDirectionalLight
    )
{
    float NearZ = InCamera->GetNearZ();
    float FarZ = InCamera->GetFarZ();
    float Fov = InCamera->GetFovY();

    FCascadeShadowMapData CascadeShadowMapData;
    CascadeShadowMapData.SplitNum = SplitNum;

    for (int i = 0; i < SplitNum; i++)
    {
        // Split distance for cascade i (i+1 to exclude near plane at i=0)
        float LogarithmicSplitDistance = NearZ *
            pow(
                (FarZ / NearZ),
                static_cast<float>(i + 1) / static_cast<float>(SplitNum)
                );

        float UniformSplitDistance = NearZ +
            ((FarZ - NearZ) * static_cast<float>(i + 1)) / static_cast<float>(SplitNum);

        float BlendedSplitDistance =
            (1.0f - SplitBlendFactor) * LogarithmicSplitDistance +
                SplitBlendFactor * UniformSplitDistance;

        CascadeShadowMapData.SplitDistance[i].X = BlendedSplitDistance;
    }

    FCascadeSubFrustum CascadeFrustum;

    FMatrix CameraViewInverse = InCamera->GetFViewProjConstantsInverse().View;
    CascadeShadowMapData.View = FMatrix::RotationMatrix(
        InDirectionalLight->GetWorldRotation()
        ).Transpose();

    // 2. Light direction 기준으로 view matrix 생성
    // FVector LightDir = InDirectionalLight->GetForwardVector();
    // if (LightDir.Length() < 1e-6f)
    //     LightDir = FVector(0, 0, -1);
    // else
    //     LightDir = LightDir.GetNormalized();
    //
    // FVector SceneCenter = FVector::ZeroVector();
    // float SceneRadius = FarZ;
    //
    // // Light position은 scene 중심에서 light direction 반대로 충분히 멀리
    // FVector LightPos = SceneCenter - LightDir * (SceneRadius + 50.0f);
    //
    // // Up vector 계산 (Z-Up, X-Forward, Y-Right Left-Handed 좌표계)
    // FVector Up = FVector(0, 0, 1);  // Z-Up
    // if (std::abs(LightDir.Z) > 0.99f)  // Light가 거의 수직(Z축과 평행)이면
    //     Up = FVector(1, 0, 0);  // X-Forward를 fallback으로
    //
    // CascadeShadowMapData.View = FMatrix::CreateLookAtLH(LightPos, SceneCenter, Up);
    //
    FMatrix CameraViewToCascadeView = CameraViewInverse * CascadeShadowMapData.View;

    float NearPlaneXY = CalculateFrustumXYWithZ(NearZ, Fov);
    CascadeFrustum.NearPlane[(int)EPlaneVertexPos::TOP_LEFT] =
        FVector4(
            -NearPlaneXY,
            NearPlaneXY,
            NearZ,
            1.0f
            ) * CameraViewToCascadeView;
    CascadeFrustum.NearPlane[(int)EPlaneVertexPos::TOP_RIGHT] =
        FVector4(
            NearPlaneXY,
            NearPlaneXY,
            NearZ,
            1.0f
            ) * CameraViewToCascadeView;
    CascadeFrustum.NearPlane[(int)EPlaneVertexPos::BOTTOM_LEFT] =
        FVector4(
            -NearPlaneXY,
            -NearPlaneXY,
            NearZ,
            1.0f
            ) * CameraViewToCascadeView;
    CascadeFrustum.NearPlane[(int)EPlaneVertexPos::BOTTOM_RIGHT] =
        FVector4(
            NearPlaneXY,
            -NearPlaneXY,
            NearZ,
            1.0f
            ) * CameraViewToCascadeView;

    for (int i = 0; i < SplitNum; i++)
    {
        float PlaneZ = CascadeShadowMapData.SplitDistance[i].X;
        float PlaneXY = CalculateFrustumXYWithZ(PlaneZ, Fov);

        CascadeFrustum.SubFarPlane[i][(int)EPlaneVertexPos::TOP_LEFT] =
        FVector4(
            -PlaneXY,
            PlaneXY,
            PlaneZ,
            1.0f) * CameraViewToCascadeView;
        CascadeFrustum.SubFarPlane[i][(int)EPlaneVertexPos::TOP_RIGHT] =
            FVector4(
                PlaneXY,
                PlaneXY,
                PlaneZ,
                1.0f) * CameraViewToCascadeView;
        CascadeFrustum.SubFarPlane[i][(int)EPlaneVertexPos::BOTTOM_LEFT] =
            FVector4(
                -PlaneXY,
                -PlaneXY,
                PlaneZ,
                1.0f) * CameraViewToCascadeView;
        CascadeFrustum.SubFarPlane[i][(int)EPlaneVertexPos::BOTTOM_RIGHT] =
            FVector4(
                PlaneXY,
                -PlaneXY,
                PlaneZ,
                1.0f) * CameraViewToCascadeView;
    }

    FVector4* NearPlane = CascadeFrustum.NearPlane;
    FVector4* FarPlane;
    
    for (int i = 0; i < SplitNum; i++)
    {
        FarPlane = CascadeFrustum.SubFarPlane[i];
        
        FVector SubFrustumMin(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector SubFrustumMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (int j = 0; j < 4; j++)
        {
            SubFrustumMin.X = std::min(SubFrustumMin.X, NearPlane[j].X);
            SubFrustumMin.X = std::min(SubFrustumMin.X, FarPlane[j].X);

            SubFrustumMin.Y = std::min(SubFrustumMin.Y, NearPlane[j].Y);
            SubFrustumMin.Y = std::min(SubFrustumMin.Y, FarPlane[j].Y);

            SubFrustumMin.Z = std::min(SubFrustumMin.Z, NearPlane[j].Z);
            SubFrustumMin.Z = std::min(SubFrustumMin.Z, FarPlane[j].Z);

            SubFrustumMax.X = std::max(SubFrustumMax.X, NearPlane[j].X);
            SubFrustumMax.X = std::max(SubFrustumMax.X, FarPlane[j].X);

            SubFrustumMax.Y = std::max(SubFrustumMax.Y, NearPlane[j].Y);
            SubFrustumMax.Y = std::max(SubFrustumMax.Y, FarPlane[j].Y);

            SubFrustumMax.Z = std::max(SubFrustumMax.Z, NearPlane[j].Z);
            SubFrustumMax.Z = std::max(SubFrustumMax.Z, FarPlane[j].Z);
        }

        // Make Crop Matrix
        float Left = SubFrustumMin.X;
        float Right = SubFrustumMax.X;
        float Bottom = SubFrustumMin.Y;
        float Top = SubFrustumMax.Y;
        float Near = SubFrustumMin.Z;// - LightViewVolumeZNearBias;
        float Far = SubFrustumMax.Z;

        CascadeShadowMapData.Proj[i] = FMatrix::CreateOrthoLH(
            Left,
            Right,
            Bottom,
            Top,
            Near,
            Far
            );

        NearPlane = CascadeFrustum.SubFarPlane[i];
    }

    return CascadeShadowMapData;
}