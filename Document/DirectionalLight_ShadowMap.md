# Directional Light Shadow Mapping 구현 가이드

**작성일**: 2025-10-25
**버전**: 1.0
**구현자**: Week8 Team5

---

## 목차

1. [개요](#개요)
2. [Shadow Mapping 기본 원리](#shadow-mapping-기본-원리)
3. [구현 단계](#구현-단계)
4. [핵심 코드 설명](#핵심-코드-설명)
5. [동작 원리](#동작-원리)
6. [트러블슈팅](#트러블슈팅)
7. [성능 최적화](#성능-최적화)
8. [테스트 방법](#테스트-방법)
9. [참고 자료](#참고-자료)

---

## 개요

### 목적

FutureEngine에 **Directional Light용 Shadow Mapping**을 구현하여 태양광과 같은 평행광원에서 사실적인 그림자를 렌더링합니다.

### 주요 특징

- **Orthographic Projection**: 평행광 특성을 반영한 직교 투영
- **PCF (Percentage Closer Filtering)**: 3×3 커널을 사용한 부드러운 그림자 경계
- **2048×2048 해상도**: 고품질 shadow map
- **Dynamic Shadow Bias**: Shadow acne 방지를 위한 GPU 자동 slope 계산
- **Rasterizer State 캐싱**: 매 프레임 GPU 리소스 생성 방지

### 기술 스펙

| 항목 | 값 |
|------|-----|
| Shadow Map 해상도 | 2048×2048 |
| Depth Format | DXGI_FORMAT_R32_TYPELESS (32-bit float) |
| DSV Format | DXGI_FORMAT_D32_FLOAT |
| SRV Format | DXGI_FORMAT_R32_FLOAT |
| Projection Type | Orthographic (Left-Handed) |
| PCF Kernel | 3×3 (9 samples) |
| Default Bias | 0.001f |
| Default Slope Bias | 1.0f |

---

## Shadow Mapping 기본 원리

### Two-Pass Rendering

Shadow mapping은 **2-pass 렌더링 기법**입니다:

```
Pass 1 (Shadow Map Pass)
Light 시점에서 scene 렌더링 → Depth 저장
        ↓
Pass 2 (Main Rendering Pass)
Camera 시점에서 렌더링 시 Light 공간의 depth와 비교
        ↓
    Shadow 판정
```

### Depth 비교 원리

각 픽셀에서:

1. **World Position → Light Space 변환**
   ```
   LightSpacePos = WorldPos × LightViewProjection
   ```

2. **Depth 비교**
   ```
   if (현재 픽셀 depth > Shadow Map depth)
       → 그림자 (다른 물체가 가림)
   else
       → 조명 받음
   ```

3. **PCF로 부드럽게**
   ```
   주변 9개 샘플의 평균으로 soft shadow
   ```

---

## 구현 단계

### Phase 1: Shadow Map 렌더링 인프라

**목적**: Light 시점에서 depth를 렌더링할 수 있는 기반 구축

#### 1-1. Shadow Map 리소스 생성

**파일**: `Engine/Source/Texture/Public/ShadowMapResources.h`

```cpp
struct FShadowMapResource
{
    ComPtr<ID3D11Texture2D> ShadowTexture = nullptr;
    ComPtr<ID3D11DepthStencilView> ShadowDSV = nullptr;
    ComPtr<ID3D11ShaderResourceView> ShadowSRV = nullptr;
    D3D11_VIEWPORT ShadowViewport = {};
    uint32 Resolution = 1024;

    void Initialize(ID3D11Device* Device, uint32 InResolution);
    void Release();
    bool IsValid() const;
};
```

**핵심 포인트**:
- **TYPELESS 포맷**: 하나의 texture를 DSV와 SRV로 동시 사용
- **32-bit Float Depth**: 높은 depth precision

#### 1-2. Depth-Only 셰이더

**파일**: `Engine/Asset/Shader/DepthOnlyVS.hlsl`

```hlsl
cbuffer CameraConstants : register(b0)
{
    matrix View;
    matrix Projection;
    // ...
}

cbuffer ModelConstants : register(b1)
{
    matrix World;
}

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT main(VS_INPUT Input)
{
    VS_OUTPUT Output;

    // World → View → Projection
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
    float4 ViewPos = mul(WorldPos, View);
    Output.Position = mul(ViewPos, Projection);

    return Output;
}
```

**핵심**: Pixel Shader 없이 depth만 출력

#### 1-3. ShadowMapPass 기본 구조

**파일**: `Engine/Source/Render/RenderPass/Public/ShadowMapPass.h`

```cpp
class FShadowMapPass : public FRenderPass
{
public:
    void Execute(FRenderingContext& Context) override;
    void Release() override;

    FShadowMapResource* GetDirectionalShadowMap(UDirectionalLightComponent* Light);

private:
    void RenderDirectionalShadowMap(UDirectionalLightComponent* Light,
        const TArray<UStaticMeshComponent*>& Meshes);

    void CalculateDirectionalLightViewProj(UDirectionalLightComponent* Light,
        const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj);

    // Shadow map storage
    TMap<UDirectionalLightComponent*, FShadowMapResource*> DirectionalShadowMaps;

    // Pipeline states
    ID3D11VertexShader* DepthOnlyShader = nullptr;
    ID3D11InputLayout* DepthOnlyInputLayout = nullptr;
    ID3D11DepthStencilState* ShadowDepthStencilState = nullptr;
    ID3D11RasterizerState* ShadowRasterizerState = nullptr;
};
```

---

### Phase 2: Light View-Projection 계산

**목적**: Light 시점의 카메라 행렬 계산

#### 2-1. Scene AABB 계산

**파일**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
void FShadowMapPass::CalculateDirectionalLightViewProj(UDirectionalLightComponent* Light,
    const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj)
{
    // 1. 모든 메시의 AABB 계산
    FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    bool bHasValidMeshes = false;
    for (auto Mesh : Meshes)
    {
        if (!Mesh->IsVisible())
            continue;

        FVector WorldMin, WorldMax;
        Mesh->GetWorldAABB(WorldMin, WorldMax);

        MinBounds.X = std::min(MinBounds.X, WorldMin.X);
        MinBounds.Y = std::min(MinBounds.Y, WorldMin.Y);
        MinBounds.Z = std::min(MinBounds.Z, WorldMin.Z);

        MaxBounds.X = std::max(MaxBounds.X, WorldMax.X);
        MaxBounds.Y = std::max(MaxBounds.Y, WorldMax.Y);
        MaxBounds.Z = std::max(MaxBounds.Z, WorldMax.Z);

        bHasValidMeshes = true;
    }

    if (!bHasValidMeshes)
    {
        OutView = FMatrix::Identity();
        OutProj = FMatrix::Identity();
        return;
    }
```

#### 2-2. View Matrix 생성

```cpp
    // 2. Light direction 기준으로 view matrix 생성
    FVector LightDir = Light->GetForwardVector();
    if (LightDir.Length() < 1e-6f)
        LightDir = FVector(0, 0, -1);
    else
        LightDir = LightDir.GetNormalized();

    FVector SceneCenter = (MinBounds + MaxBounds) * 0.5f;
    float SceneRadius = (MaxBounds - MinBounds).Length() * 0.5f;

    // Light position: scene 중심에서 light direction 반대로 충분히 멀리
    FVector LightPos = SceneCenter - LightDir * (SceneRadius + 50.0f);

    // Up vector 계산 (Z-Up, X-Forward, Y-Right Left-Handed 좌표계)
    FVector Up = FVector(0, 0, 1);  // Z-Up
    if (std::abs(LightDir.Z) > 0.99f)  // Light가 거의 수직이면
        Up = FVector(1, 0, 0);  // X-Forward를 fallback으로

    OutView = ShadowMatrixHelper::CreateLookAtLH(LightPos, SceneCenter, Up);
```

**핵심 포인트**:
- **Z-Up 좌표계**: Engine의 좌표계는 Z-Up, X-Forward
- **Gimbal Lock 회피**: Light가 Z축과 평행하면 Up을 X축으로 변경
- **충분한 거리**: Light를 scene 외부에 배치하여 모든 geometry 포함

#### 2-3. Orthographic Projection 생성

```cpp
    // 3. AABB를 light view space로 변환하여 orthographic projection 범위 계산
    FVector LightSpaceMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector LightSpaceMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // AABB의 8개 코너를 light view space로 변환
    FVector Corners[8] = {
        FVector(MinBounds.X, MinBounds.Y, MinBounds.Z),
        FVector(MaxBounds.X, MinBounds.Y, MinBounds.Z),
        FVector(MinBounds.X, MaxBounds.Y, MinBounds.Z),
        FVector(MaxBounds.X, MaxBounds.Y, MinBounds.Z),
        FVector(MinBounds.X, MinBounds.Y, MaxBounds.Z),
        FVector(MaxBounds.X, MinBounds.Y, MaxBounds.Z),
        FVector(MinBounds.X, MaxBounds.Y, MaxBounds.Z),
        FVector(MaxBounds.X, MaxBounds.Y, MaxBounds.Z)
    };

    for (int i = 0; i < 8; i++)
    {
        FVector4 LightSpaceCorner = OutView.TransformVector4(FVector4(Corners[i], 1.0f));

        LightSpaceMin.X = std::min(LightSpaceMin.X, LightSpaceCorner.X);
        LightSpaceMin.Y = std::min(LightSpaceMin.Y, LightSpaceCorner.Y);
        LightSpaceMin.Z = std::min(LightSpaceMin.Z, LightSpaceCorner.Z);

        LightSpaceMax.X = std::max(LightSpaceMax.X, LightSpaceCorner.X);
        LightSpaceMax.Y = std::max(LightSpaceMax.Y, LightSpaceCorner.Y);
        LightSpaceMax.Z = std::max(LightSpaceMax.Z, LightSpaceCorner.Z);
    }

    // 4. Orthographic projection matrix 생성
    OutProj = ShadowMatrixHelper::CreateOrthoLH(
        LightSpaceMin.X, LightSpaceMax.X,
        LightSpaceMin.Y, LightSpaceMax.Y,
        LightSpaceMin.Z, LightSpaceMax.Z
    );
}
```

**핵심 포인트**:
- **Tight Fitting**: AABB의 8개 코너를 모두 변환하여 최소 projection 범위 계산
- **Orthographic**: Directional light는 평행광이므로 원근 투영 불필요

---

### Phase 3: Shadow Map 렌더링

**목적**: Light 시점에서 depth 렌더링

#### 3-1. RenderDirectionalShadowMap 구현

**파일**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
void FShadowMapPass::RenderDirectionalShadowMap(UDirectionalLightComponent* Light,
    const TArray<UStaticMeshComponent*>& Meshes)
{
    const auto& Renderer = URenderer::GetInstance();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

    // 1. Shadow map 리소스 준비
    FShadowMapResource* ShadowMap = nullptr;
    auto It = DirectionalShadowMaps.find(Light);
    if (It == DirectionalShadowMaps.end())
    {
        ShadowMap = new FShadowMapResource();
        ShadowMap->Initialize(Renderer.GetDevice(), Light->GetShadowResolution());
        DirectionalShadowMaps[Light] = ShadowMap;
    }
    else
    {
        ShadowMap = It->second;
        if (ShadowMap->NeedsResize(Light->GetShadowResolution()))
        {
            ShadowMap->Release();
            ShadowMap->Initialize(Renderer.GetDevice(), Light->GetShadowResolution());
        }
    }

    // 현재 RenderTarget과 DepthStencil 백업
    ID3D11RenderTargetView* OriginalRTV = nullptr;
    ID3D11DepthStencilView* OriginalDSV = nullptr;
    DeviceContext->OMGetRenderTargets(1, &OriginalRTV, &OriginalDSV);

    D3D11_VIEWPORT OriginalViewport;
    UINT NumViewports = 1;
    DeviceContext->RSGetViewports(&NumViewports, &OriginalViewport);

    // Shadow map으로 전환
    ID3D11RenderTargetView* NullRTV = nullptr;
    Pipeline->SetRenderTargets(1, &NullRTV, ShadowMap->ShadowDSV.Get());
    DeviceContext->ClearDepthStencilView(ShadowMap->ShadowDSV.Get(),
        D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Viewport 설정 (DeviceContext 직접 사용, Pipeline API가 viewport 미지원)
    DeviceContext->RSSetViewports(1, &ShadowMap->ShadowViewport);

    // 2. Light별 캐싱된 rasterizer state 가져오기
    ID3D11RasterizerState* RastState = GetOrCreateRasterizerState(Light);

    // 3. Pipeline 설정
    FPipelineInfo ShadowPipelineInfo = {
        DepthOnlyInputLayout,
        DepthOnlyShader,
        RastState,
        ShadowDepthStencilState,
        nullptr,  // No pixel shader
        nullptr,  // No blend state
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };
    Pipeline->UpdatePipeline(ShadowPipelineInfo);

    // 4. Light view-projection 계산
    FMatrix LightView, LightProj;
    CalculateDirectionalLightViewProj(Light, Meshes, LightView, LightProj);

    // Store calculated matrix in light component
    FMatrix LightViewProj = LightView * LightProj;
    Light->SetShadowViewProjection(LightViewProj);

    // 5. 각 메시 렌더링
    for (auto Mesh : Meshes)
    {
        if (Mesh->IsVisible())
        {
            RenderMeshDepth(Mesh, LightView, LightProj);
        }
    }

    // 6. 상태 복원
    Pipeline->SetRenderTargets(1, &OriginalRTV, OriginalDSV);
    DeviceContext->RSSetViewports(1, &OriginalViewport);

    if (OriginalRTV)
        OriginalRTV->Release();
    if (OriginalDSV)
        OriginalDSV->Release();
}
```

**핵심 포인트**:
- **리소스 캐싱**: 한 번 생성된 shadow map은 재사용
- **해상도 동적 변경**: Light 설정이 바뀌면 재생성
- **State 백업/복원**: 기존 rendering state를 보존

#### 3-2. RenderMeshDepth 구현

```cpp
void FShadowMapPass::RenderMeshDepth(UStaticMeshComponent* Mesh,
    const FMatrix& LightView, const FMatrix& LightProj)
{
    const auto& Renderer = URenderer::GetInstance();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

    // Camera constants (Light의 view/proj 사용)
    FCameraConstants CameraConst;
    CameraConst.View = LightView;
    CameraConst.Projection = LightProj;
    DeviceContext->UpdateSubresource(ConstantBufferCamera, 0, nullptr, &CameraConst, 0, 0);

    // Model constants (Mesh의 world transform)
    FModelConstants ModelConst;
    ModelConst.World = Mesh->GetOwner()->GetTransform().ToMatrix();
    DeviceContext->UpdateSubresource(ConstantBufferModel, 0, nullptr, &ModelConst, 0, 0);

    // Mesh rendering
    // ... vertex/index buffer binding and draw call ...
}
```

---

### Phase 4: Shadow Sampling (Main Rendering)

**목적**: Camera 시점에서 shadow map을 샘플링하여 그림자 적용

#### 4-1. Shadow Map Texture 및 Sampler 선언

**파일**: `Engine/Asset/Shader/UberLit.hlsl`

```hlsl
// Shadow map resources
Texture2D DirectionalShadowMap : register(t10);
SamplerComparisonState ShadowSampler : register(s1);
```

#### 4-2. Comparison Sampler 생성

**파일**: `Engine/Source/Render/Renderer/Private/Renderer.cpp`

```cpp
void URenderer::CreateSamplerState()
{
    // ... 기존 sampler ...

    // Comparison sampler for shadow mapping (PCF)
    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.BorderColor[0] = 1.0f;  // Outside = lit (no shadow)
    shadowSamplerDesc.BorderColor[1] = 1.0f;
    shadowSamplerDesc.BorderColor[2] = 1.0f;
    shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.MinLOD = 0;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    GetDevice()->CreateSamplerState(&shadowSamplerDesc, &ShadowComparisonSampler);
}
```

**핵심 포인트**:
- **COMPARISON Filter**: GPU가 자동으로 depth 비교 수행
- **BORDER 모드**: Shadow map 영역 밖은 조명 받음 (1.0f)
- **LESS_EQUAL**: 현재 depth ≤ shadow map depth면 lit

#### 4-3. Shadow Factor 계산 함수

**파일**: `Engine/Asset/Shader/UberLit.hlsl`

```hlsl
float CalculateDirectionalShadowFactor(FDirectionalLightInfo Light, float3 WorldPos)
{
    // Shadow casting이 비활성화되어 있으면 그림자 없음
    if (Light.CastShadow == 0)
        return 1.0f;

    // 1. World space → Light space 변환
    float4 LightSpacePos = mul(float4(WorldPos, 1.0f), Light.LightViewProjection);
    LightSpacePos.xyz /= LightSpacePos.w;  // Perspective divide (orthographic에서도 w=1)

    // 2. Frustum culling: Light frustum 밖이면 그림자 없음
    if (LightSpacePos.x < -1.0f || LightSpacePos.x > 1.0f ||
        LightSpacePos.y < -1.0f || LightSpacePos.y > 1.0f ||
        LightSpacePos.z < 0.0f || LightSpacePos.z > 1.0f)
        return 1.0f;

    // 3. NDC [-1,1] → Texture coordinates [0,1]
    float2 ShadowTexCoord;
    ShadowTexCoord.x = LightSpacePos.x * 0.5f + 0.5f;
    ShadowTexCoord.y = -LightSpacePos.y * 0.5f + 0.5f;  // Y flip (NDC와 Texture 좌표계 차이)

    float CurrentDepth = LightSpacePos.z;

    // 4. 3×3 PCF (Percentage Closer Filtering)
    float ShadowFactor = 0.0f;
    float2 TexelSize = 1.0f / 2048.0f;  // Shadow map resolution

    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            float2 Offset = float2(x, y) * TexelSize;
            // SampleCmpLevelZero: GPU가 CurrentDepth와 shadow map을 비교
            // 결과: 0.0f (shadow) or 1.0f (lit)
            ShadowFactor += DirectionalShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                ShadowTexCoord + Offset,
                CurrentDepth
            );
        }
    }

    return ShadowFactor / 9.0f;  // 평균 (0.0 = 완전히 그림자, 1.0 = 완전히 조명)
}
```

**핵심 포인트**:
- **PCF**: 9개 샘플의 평균으로 soft shadow edge
- **SampleCmpLevelZero**: GPU 하드웨어가 depth 비교 수행
- **Y-flip**: NDC와 Texture UV 좌표계의 Y축 방향이 반대

#### 4-4. Lighting 계산에 Shadow 적용

**파일**: `Engine/Asset/Shader/UberLit.hlsl`

```hlsl
FIllumination CalculateDirectionalLight(FDirectionalLightInfo Info, float3 WorldNormal, float3 WorldPos)
{
    FIllumination Result = (FIllumination) 0;

    float3 LightDir = SafeNormalize3(-Info.Direction);
    float NdotL = saturate(dot(WorldNormal, LightDir));

    // Shadow factor 계산
    float ShadowFactor = CalculateDirectionalShadowFactor(Info, WorldPos);

    // Shadow 적용
    Result.Diffuse = Info.Color * Info.Intensity * NdotL * ShadowFactor;

    // Specular도 shadow 적용
    float3 ViewDir = SafeNormalize3(ViewWorldLocation - WorldPos);
    float3 HalfVec = SafeNormalize3(LightDir + ViewDir);
    float NdotH = saturate(dot(WorldNormal, HalfVec));
    float Spec = pow(NdotH, Ns);

    Result.Specular = Info.Color * Info.Intensity * Spec * ShadowFactor;

    return Result;
}
```

#### 4-5. StaticMeshPass에서 Shadow Map Binding

**파일**: `Engine/Source/Render/RenderPass/Private/StaticMeshPass.cpp`

```cpp
void FStaticMeshPass::Execute(FRenderingContext& Context)
{
    // ... pipeline setup ...

    // Bind shadow comparison sampler
    Pipeline->SetSamplerState(1, EShaderType::PS, Renderer.GetShadowComparisonSampler());

    // Bind directional light shadow map if available
    if (!Context.DirectionalLights.empty() && Context.DirectionalLights[0]->GetCastShadows())
    {
        UDirectionalLightComponent* DirLight = Context.DirectionalLights[0];
        FShadowMapPass* ShadowPass = Renderer.GetShadowMapPass();
        if (ShadowPass)
        {
            FShadowMapResource* ShadowMap = ShadowPass->GetDirectionalShadowMap(DirLight);
            if (ShadowMap && ShadowMap->IsValid())
            {
                Pipeline->SetShaderResourceView(10, EShaderType::PS, ShadowMap->ShadowSRV.Get());
            }
        }
    }

    // ... mesh rendering ...
}
```

#### 4-6. Shadow Matrix를 Light Component에 캐싱

**파일**: `Engine/Source/Component/Public/DirectionalLightComponent.h`

```cpp
class UDirectionalLightComponent : public ULightComponentBase
{
    // ...

    // Shadow mapping
    void SetShadowViewProjection(const FMatrix& ViewProj) { CachedShadowViewProjection = ViewProj; }
    const FMatrix& GetShadowViewProjection() const { return CachedShadowViewProjection; }

private:
    FMatrix CachedShadowViewProjection = FMatrix::Identity();
};
```

**파일**: `Engine/Source/Component/Private/DirectionalLightComponent.cpp`

```cpp
FDirectionalLightInfo UDirectionalLightComponent::GetDirectionalLightInfo() const
{
    FDirectionalLightInfo Info;
    // ... 다른 필드 ...

    // Shadow parameters
    Info.LightViewProjection = CachedShadowViewProjection;  // ShadowMapPass에서 업데이트됨
    Info.CastShadow = GetCastShadows() ? 1u : 0u;
    Info.ShadowBias = GetShadowBias();
    Info.ShadowSlopeBias = GetShadowSlopeBias();
    Info.ShadowSharpen = GetShadowSharpen();

    return Info;
}
```

---

## 핵심 코드 설명

### 좌표계 변환 흐름

```
World Space (Engine: Z-Up, X-Forward, Y-Right LH)
    ↓ CreateLookAtLH (View Matrix)
View Space (DirectX Standard: Z-Forward)
    ↓ Projection Matrix (Ortho or Perspective)
NDC Space (DirectX: X,Y ∈ [-1,1], Z ∈ [0,1])
    ↓ Viewport Transform
Screen Space (Pixel Coordinates)
```

**중요**: 엔진 좌표계는 Z-Up이지만, View Matrix (CreateLookAtLH)가 DirectX 표준 좌표계로 변환하므로 셰이더는 항상 DirectX 좌표계에서 동작합니다.

### Shadow Acne 방지: DepthBias

**공식**:
```
FinalDepth = OriginalDepth + DepthBias × r + SlopeScaledDepthBias × MaxSlope
```

**파라미터**:
- `r`: Depth buffer의 최소 표현 단위 (format dependent)
- `MaxSlope`: `max(|dz/dx|, |dz/dy|)` - GPU가 자동 계산
- `DepthBias`: CPU에서 설정 (예: 0.001f → 100으로 스케일)
- `SlopeScaledDepthBias`: CPU에서 설정 (예: 1.0f)

**동작 원리**:
1. 표면이 light와 평행할수록 slope가 커짐
2. GPU가 자동으로 각 픽셀의 slope 계산
3. 기울어진 표면일수록 더 큰 bias 자동 적용
4. Shadow acne (자기 그림자 아티팩트) 방지

**구현**:

```cpp
// ShadowMapPass.cpp - GetOrCreateRasterizerState()
RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();
```

---

## 동작 원리

### 전체 렌더링 파이프라인

```
1. ShadowMapPass::Execute()
   ├─ DirectionalLights 순회
   ├─ RenderDirectionalShadowMap()
   │  ├─ CalculateDirectionalLightViewProj()
   │  │  ├─ Scene AABB 계산
   │  │  ├─ Light View Matrix 생성
   │  │  └─ Orthographic Projection 생성
   │  ├─ Shadow Map 리소스 준비
   │  ├─ Pipeline state 설정 (DepthOnly shader, DepthBias)
   │  ├─ Light에 ViewProj matrix 저장
   │  └─ 각 Mesh depth 렌더링
   └─ Shadow Map 생성 완료

2. StaticMeshPass::Execute()
   ├─ Shadow Map SRV와 Comparison Sampler Binding
   ├─ Light info (LightViewProjection 포함) GPU 전송
   └─ UberLit shader 실행
      ├─ CalculateDirectionalShadowFactor()
      │  ├─ World → Light space 변환
      │  ├─ Frustum culling
      │  ├─ NDC → Texture coordinate 변환
      │  └─ 3×3 PCF sampling
      └─ Shadow factor를 lighting에 곱함
```

### Resource Hazard 방지

**문제**: Shadow map이 동시에 DSV(쓰기)와 SRV(읽기)로 바인딩되면 D3D11 경고 발생

**해결**: ShadowMapPass::Execute() 시작 시 SRV unbind

```cpp
void FShadowMapPass::Execute(FRenderingContext& Context)
{
    // IMPORTANT: Unbind shadow map SRVs before rendering to them as DSV
    const auto& Renderer = URenderer::GetInstance();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->PSSetShaderResources(10, 1, &NullSRV);  // Unbind t10

    // ... shadow rendering ...
}
```

---

## 트러블슈팅

### 문제 1: Directional Light Spawn 시 엔진 멈춤

**증상**: Directional Light를 scene에 추가하면 엔진이 hang

**원인**: Vertex Shader 진입점 이름 불일치
```cpp
// DepthOnlyVS.hlsl
VS_OUTPUT main(VS_INPUT Input)  // "main"
```

```cpp
// Renderer.cpp - 셰이더 컴파일
CompileVertexShader(L"DepthOnlyVS.hlsl", "mainVS", ...)  // "mainVS" - 틀림!
```

**해결**: 진입점 이름을 "main"으로 통일

```cpp
CompileVertexShader(L"DepthOnlyVS.hlsl", "main", ...)  // 수정
```

---

### 문제 2: Shadow가 렌더링되지 않음

**증상**: Shadow map pass는 실행되지만 그림자가 보이지 않음

**원인**: Phase 4 (Shadow Sampling)이 구현되지 않음

**해결**: UberLit.hlsl에 shadow sampling 구현 (Phase 4 참고)

---

### 문제 3: D3D11 Resource Hazard Warning

**증상**:
```
D3D11 WARNING: ID3D11DeviceContext::OMSetRenderTargets: Resource being set to OM DepthStencil is still bound on input!
D3D11 WARNING: Forcing PS shader resource slot 10 to NULL.
```

**원인**: Shadow map이 SRV(t10)로 바인딩된 상태에서 DSV로 사용 시도

**해결**: ShadowMapPass::Execute() 시작 시 SRV unbind

```cpp
ID3D11ShaderResourceView* NullSRV = nullptr;
DeviceContext->PSSetShaderResources(10, 1, &NullSRV);
```

---

### 문제 4: Shadow가 잘못된 위치에 렌더링됨

**증상**: Shadow가 origin 근처 사각형 영역에만 나타남

**원인**: `LightViewProjection`이 Identity matrix로 전송됨

**근본 원인**: ShadowMapPass에서 계산한 matrix가 Light component에 저장되지 않음

**해결**:

1. **DirectionalLightComponent에 캐싱 추가**:
```cpp
class UDirectionalLightComponent
{
    void SetShadowViewProjection(const FMatrix& ViewProj);
    const FMatrix& GetShadowViewProjection() const;
private:
    FMatrix CachedShadowViewProjection = FMatrix::Identity();
};
```

2. **ShadowMapPass에서 matrix 저장**:
```cpp
FMatrix LightViewProj = LightView * LightProj;
Light->SetShadowViewProjection(LightViewProj);
```

3. **GetDirectionalLightInfo()에서 반환**:
```cpp
Info.LightViewProjection = CachedShadowViewProjection;  // not Identity!
```

---

### 문제 5: 좌표계 불일치 의심

**증상**: "엔진은 Z-Up인데 shader에서 Z를 depth로 쓰는데 괜찮나?"

**해결**: 좌표계 변환 흐름 이해

1. **World Space**: Z-Up, X-Forward (Engine 좌표계)
2. **View Matrix (CreateLookAtLH)**: DirectX 표준 좌표계로 변환
3. **View/NDC Space**: Z-Forward (DirectX 표준)
4. **Shader**: 항상 DirectX 좌표계에서 동작

**결론**: 문제 없음. View matrix가 자동으로 변환해줌.

**확인 방법**:
```cpp
// CalculateDirectionalLightViewProj()
FVector Up = FVector(0, 0, 1);  // Z-Up (엔진 좌표계)
if (std::abs(LightDir.Z) > 0.99f)  // Z축 검사 (엔진 좌표계)
    Up = FVector(1, 0, 0);
```

```hlsl
// UberLit.hlsl
if (LightSpacePos.z < 0.0f || LightSpacePos.z > 1.0f)  // Z는 depth (DirectX 좌표계)
```

---

## 성능 최적화

### 최적화 1: Rasterizer State 캐싱

**문제**: 매 프레임 rasterizer state 생성/해제 → GPU overhead

**원래 코드** (비효율):
```cpp
void FShadowMapPass::RenderDirectionalShadowMap(...)
{
    // 매 프레임 생성
    D3D11_RASTERIZER_DESC RastDesc = {};
    // ... setup ...
    ID3D11RasterizerState* TempRastState;
    Device->CreateRasterizerState(&RastDesc, &TempRastState);

    // ... rendering ...

    TempRastState->Release();  // 매 프레임 해제
}
```

**최적화된 코드**:

**헤더**:
```cpp
class FShadowMapPass
{
private:
    TMap<UDirectionalLightComponent*, ID3D11RasterizerState*> DirectionalRasterizerStates;

    ID3D11RasterizerState* GetOrCreateRasterizerState(UDirectionalLightComponent* Light);
};
```

**구현**:
```cpp
ID3D11RasterizerState* FShadowMapPass::GetOrCreateRasterizerState(UDirectionalLightComponent* Light)
{
    // 이미 생성된 state가 있으면 재사용
    auto It = DirectionalRasterizerStates.find(Light);
    if (It != DirectionalRasterizerStates.end())
        return It->second;

    // 새로 생성
    const auto& Renderer = URenderer::GetInstance();
    D3D11_RASTERIZER_DESC RastDesc = {};
    ShadowRasterizerState->GetDesc(&RastDesc);

    RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
    RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();

    ID3D11RasterizerState* NewState = nullptr;
    Renderer.GetDevice()->CreateRasterizerState(&RastDesc, &NewState);

    // 캐시에 저장
    DirectionalRasterizerStates[Light] = NewState;

    return NewState;
}

void FShadowMapPass::Release()
{
    // Cleanup
    for (auto& Pair : DirectionalRasterizerStates)
    {
        if (Pair.second)
            Pair.second->Release();
    }
    DirectionalRasterizerStates.clear();
}
```

**성능 향상**: GPU 리소스 생성 오버헤드 제거

---

### 최적화 2: Shadow Map 리소스 재사용

```cpp
// Shadow map은 한 번만 생성하고 재사용
auto It = DirectionalShadowMaps.find(Light);
if (It == DirectionalShadowMaps.end())
{
    // 첫 생성
    ShadowMap = new FShadowMapResource();
    ShadowMap->Initialize(Device, Resolution);
    DirectionalShadowMaps[Light] = ShadowMap;
}
else
{
    ShadowMap = It->second;

    // 해상도가 변경되었을 때만 재생성
    if (ShadowMap->NeedsResize(Resolution))
    {
        ShadowMap->Release();
        ShadowMap->Initialize(Device, Resolution);
    }
}
```

---

### 최적화 3: PCF Kernel Size

**현재**: 3×3 (9 samples)

**Trade-off**:
- 더 큰 커널 (5×5, 7×7) → 더 부드러운 shadow, 하지만 느림
- 더 작은 커널 (1×1) → 빠름, 하지만 hard edge

**권장**: 3×3이 품질과 성능의 좋은 균형

---

## 테스트 방법

### 1. 기본 Shadow 테스트

1. Scene에 **Directional Light** 추가
2. Light의 **Cast Shadows** 활성화
3. **Rotation**을 조정하여 light 방향 변경
4. Ground plane과 몇 개의 cube를 배치
5. **Play** 실행
6. Shadow가 light 방향 반대로 생기는지 확인

### 2. Shadow Bias 조정

**Shadow Acne** (떨림 현상) 발생 시:
- `ShadowBias` 증가 (0.001 → 0.002)
- `ShadowSlopeBias` 증가 (1.0 → 2.0)

**Peter Panning** (그림자가 물체에서 떨어짐) 발생 시:
- `ShadowBias` 감소
- `ShadowSlopeBias` 감소

### 3. 해상도 테스트

Shadow map 해상도 변경:
- 512×512: 낮은 품질, 빠름
- 1024×1024: 중간 품질
- 2048×2048: 높은 품질 (기본값)
- 4096×4096: 최고 품질, 느림

### 4. 다중 Light 테스트

**현재 제한**: 첫 번째 Directional Light만 shadow casting

**확장 방법**:
```cpp
// StaticMeshPass.cpp
for (int i = 0; i < Context.DirectionalLights.size(); i++)
{
    auto DirLight = Context.DirectionalLights[i];
    if (DirLight->GetCastShadows())
    {
        // Bind to t10+i
        Pipeline->SetShaderResourceView(10 + i, EShaderType::PS, ...);
    }
}
```

---

## 참고 자료

### 관련 파일

**Shadow Map Pass**:
- `Engine/Source/Render/RenderPass/Public/ShadowMapPass.h`
- `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

**Shadow Map 리소스**:
- `Engine/Source/Texture/Public/ShadowMapResources.h`
- `Engine/Source/Texture/Private/ShadowMapResources.cpp`

**셰이더**:
- `Engine/Asset/Shader/DepthOnlyVS.hlsl` - Depth-only vertex shader
- `Engine/Asset/Shader/UberLit.hlsl` - Shadow sampling shader

**Light Component**:
- `Engine/Source/Component/Public/DirectionalLightComponent.h`
- `Engine/Source/Component/Private/DirectionalLightComponent.cpp`

**Rendering**:
- `Engine/Source/Render/RenderPass/Private/StaticMeshPass.cpp` - Shadow map binding
- `Engine/Source/Render/Renderer/Private/Renderer.cpp` - Comparison sampler

**Core Types**:
- `Engine/Source/Global/CoreTypes.h` - `FDirectionalLightInfo` 구조체

### 외부 참고 자료

- **Microsoft DirectX 11 Documentation**:
  - Shadow Mapping: https://docs.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps
  - Comparison Sampling: https://docs.microsoft.com/en-us/windows/win32/direct3d11/d3d11-graphics-programming-guide-resources-sample

- **Real-Time Rendering (4th Edition)**:
  - Chapter 7: Shadows

- **GPU Gems**:
  - Chapter 11: Shadow Map Antialiasing

---

## 구현 체크리스트

- [x] **Phase 1**: Shadow Map 렌더링 인프라
  - [x] FShadowMapResource 구현
  - [x] DepthOnlyVS.hlsl 작성
  - [x] ShadowMapPass 기본 구조
  - [x] Shadow pipeline states 생성

- [x] **Phase 2**: Light View-Projection 계산
  - [x] Scene AABB 계산
  - [x] CreateLookAtLH 구현
  - [x] CreateOrthoLH 구현
  - [x] CalculateDirectionalLightViewProj 구현

- [x] **Phase 3**: Shadow Map 렌더링
  - [x] RenderDirectionalShadowMap 구현
  - [x] RenderMeshDepth 구현
  - [x] Resource caching
  - [x] State backup/restore

- [x] **Phase 4**: Shadow Sampling
  - [x] Shadow map texture binding (t10)
  - [x] Comparison sampler 생성 (s1)
  - [x] CalculateDirectionalShadowFactor 구현
  - [x] CalculateDirectionalLight에 shadow 적용
  - [x] LightViewProjection 캐싱

- [x] **최적화**
  - [x] Rasterizer state 캐싱
  - [x] Shadow map 리소스 재사용
  - [x] Resource hazard 방지

- [x] **트러블슈팅**
  - [x] Vertex shader 진입점 수정
  - [x] Shadow matrix 전달 수정
  - [x] Resource unbinding 추가
  - [x] 좌표계 검증

---

## 향후 개선 사항

### 1. Cascaded Shadow Maps (CSM)

**목적**: 넓은 view frustum에서 일정한 shadow 품질 유지

**방법**: View frustum을 여러 cascade로 분할, 각각 별도의 shadow map

### 2. Soft Shadows (PCSS)

**목적**: 거리에 따른 자연스러운 soft shadow

**방법**: Percentage Closer Soft Shadows (PCSS) 구현

### 3. Variance Shadow Maps (VSM)

**목적**: PCF보다 효율적인 soft shadow

**방법**: Depth 대신 depth의 평균과 분산 저장

### 4. 다중 Directional Light Shadow

**현재**: 첫 번째 light만 shadow casting

**개선**: 여러 directional light의 shadow 지원

---

**문서 끝**

이 문서는 FutureEngine의 Directional Light Shadow Mapping 구현을 완전히 설명합니다. 질문이나 추가 설명이 필요하면 Week8 Team5에 문의하세요.
