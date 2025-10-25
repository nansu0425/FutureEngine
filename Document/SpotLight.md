# Spot Light Shadow Mapping 구현 가이드

> FutureEngine의 Spot Light Shadow Mapping 완전 구현 문서

**작성일**: 2025-10-25
**버전**: 1.0
**구현자**: Week8 Team5

---

## 목차

1. [개요](#개요)
2. [Shadow Mapping 기본 원리](#shadow-mapping-기본-원리)
3. [DirectionalLight와의 차이점](#directionallight와의-차이점)
4. [구현 단계](#구현-단계)
5. [핵심 코드 설명](#핵심-코드-설명)
6. [동작 원리](#동작-원리)
7. [트러블슈팅](#트러블슈팅)
8. [성능 최적화](#성능-최적화)
9. [테스트 방법](#테스트-방법)
10. [참고 자료](#참고-자료)

---

## 개요

### 목적

FutureEngine에 **Spot Light용 Shadow Mapping**을 구현하여 손전등, 무대 조명과 같은 원뿔형 광원에서 사실적인 그림자를 렌더링합니다.

### 주요 특징

- **Perspective Projection**: 원뿔형 frustum 특성을 반영한 원근 투영
- **FOV-based Projection**: Light의 OuterConeAngle을 기반으로 FOV 자동 계산
- **PCF (Percentage Closer Filtering)**: 3×3 커널을 사용한 부드러운 그림자 경계
- **1024×1024 해상도**: 고품질 shadow map
- **Dynamic Shadow Bias**: Shadow acne 방지를 위한 GPU 자동 slope 계산
- **Rasterizer State 캐싱**: 매 프레임 GPU 리소스 생성 방지
- **Per-Light Shadow Map**: 각 SpotLight마다 독립적인 shadow map

### 기술 스펙

| 항목 | 값 |
|------|-----|
| Shadow Map 해상도 | 1024×1024 (기본값) |
| Depth Format | DXGI_FORMAT_R32_TYPELESS (32-bit float) |
| DSV Format | DXGI_FORMAT_D32_FLOAT |
| SRV Format | DXGI_FORMAT_R32_FLOAT |
| Projection Type | Perspective (Left-Handed) |
| FOV 계산 | OuterConeAngle × 2 |
| Aspect Ratio | 1.0 (정사각형 shadow map) |
| Near Plane | 1.0 |
| Far Plane | AttenuationRadius |
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

2. **Perspective Divide**
   ```
   LightSpacePos.xyz /= LightSpacePos.w  // Perspective projection이므로 필수
   ```

3. **Depth 비교**
   ```
   if (현재 픽셀 depth > Shadow Map depth)
       → 그림자 (다른 물체가 가림)
   else
       → 조명 받음
   ```

4. **PCF로 부드럽게**
   ```
   주변 9개 샘플의 평균으로 soft shadow
   ```

---

## DirectionalLight와의 차이점

### 핵심 차이점 비교

| 특성 | Directional Light | Spot Light |
|------|-------------------|------------|
| **Projection** | Orthographic | **Perspective** |
| **Light Position** | Scene AABB 기반 자동 계산 | **Component WorldLocation 직접 사용** |
| **Light Direction** | GetForwardVector() | **GetForwardVector()** |
| **FOV** | N/A (평행광) | **OuterConeAngle × 2** |
| **Near/Far Plane** | Scene AABB 기반 계산 | **Near=1.0, Far=AttenuationRadius** |
| **AABB 계산 필요** | Yes (projection 범위) | **No (cone frustum으로 충분)** |
| **Aspect Ratio** | Scene 비율 | **1.0 (정사각형)** |
| **View Matrix** | Scene center를 target으로 | **Light position + direction으로 target 계산** |

### 왜 Perspective인가?

**Directional Light**: 태양과 같은 평행광
- 거리에 관계없이 광선이 평행
- Orthographic projection 사용

**Spot Light**: 손전등, 무대 조명
- 한 점에서 원뿔형으로 퍼짐
- Perspective projection 사용 (멀수록 넓어짐)

```
Directional Light (Orthographic):
|||||||||  ← 평행한 광선
|||||||||
|||||||||

Spot Light (Perspective):
  \   |   /  ← 퍼지는 광선
   \  |  /
    \ | /
     \|/
      *  ← Light position
```

---

## 구현 단계

### Phase 1: Perspective Projection Matrix 헬퍼 함수

**목적**: SpotLight의 cone frustum을 표현할 perspective projection matrix 생성

#### 1-1. CreatePerspectiveFovLH 구현

**파일**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
namespace ShadowMatrixHelper
{
    // Create a perspective projection matrix (Left-Handed)
    // Used for spot light shadow mapping
    FMatrix CreatePerspectiveFovLH(float FovYRadians, float Aspect, float Near, float Far)
    {
        // f = 1 / tan(fovY/2)
        const float F = 1.0f / std::tanf(FovYRadians * 0.5f);

        FMatrix Result = FMatrix::Identity();
        // | f/aspect   0        0         0 |
        // |    0       f        0         0 |
        // |    0       0   zf/(zf-zn)     1 |
        // |    0       0  -zn*zf/(zf-zn)  0 |
        Result.Data[0][0] = F / Aspect;
        Result.Data[1][1] = F;
        Result.Data[2][2] = Far / (Far - Near);
        Result.Data[2][3] = 1.0f;
        Result.Data[3][2] = (-Near * Far) / (Far - Near);
        Result.Data[3][3] = 0.0f;

        return Result;
    }
}
```

**핵심 포인트**:
- **FOV**: Field of View (Y축 기준) - 라디안 단위
- **Aspect**: Width/Height 비율 (SpotLight는 1.0 사용)
- **Near/Far**: Frustum의 앞/뒤 평면 거리
- **Left-Handed**: DirectX 좌표계

**수학적 원리**:
```
FOV (Field of View)
    ↓
    *  ← Light
   /|\
  / | \
 /  |  \
/   |   \
────────── ← Far plane
```

---

### Phase 2: Spot Light View-Projection 계산

**목적**: Spot Light 시점의 카메라 행렬 계산

#### 2-1. CalculateSpotLightViewProj 구현

**파일**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
void FShadowMapPass::CalculateSpotLightViewProj(USpotLightComponent* Light,
    const TArray<UStaticMeshComponent*>& Meshes, FMatrix& OutView, FMatrix& OutProj)
{
    // 1. Light의 위치와 방향 가져오기
    FVector LightPos = Light->GetWorldLocation();
    FVector LightDir = Light->GetForwardVector();

    // LightDir이 거의 0이면 기본값 사용
    if (LightDir.Length() < 1e-6f)
        LightDir = FVector(1, 0, 0);  // X-Forward (engine default)
    else
        LightDir = LightDir.GetNormalized();

    // 2. View Matrix 생성: Light 위치에서 Direction 방향으로
    FVector Target = LightPos + LightDir;

    // Up vector 계산 (Z-Up, X-Forward, Y-Right Left-Handed 좌표계)
    FVector Up = FVector(0, 0, 1);  // Z-Up
    if (std::abs(LightDir.Z) > 0.99f)  // Light가 거의 수직(Z축과 평행)이면
        Up = FVector(1, 0, 0);  // X-Forward를 fallback으로

    OutView = ShadowMatrixHelper::CreateLookAtLH(LightPos, Target, Up);

    // 3. Perspective Projection 생성: Cone 모양의 frustum
    float FovY = Light->GetOuterConeAngle() * 2.0f;  // Full cone angle
    float Aspect = 1.0f;  // Square shadow map
    float Near = 1.0f;    // 너무 작으면 depth precision 문제
    float Far = Light->GetAttenuationRadius();  // Light range

    // FovY가 너무 작거나 크면 clamp (유효 범위: 0.1 ~ PI - 0.1)
    FovY = std::clamp(FovY, 0.1f, PI - 0.1f);

    // Far가 Near보다 작거나 같으면 기본값 사용
    if (Far <= Near)
        Far = Near + 10.0f;

    OutProj = ShadowMatrixHelper::CreatePerspectiveFovLH(FovY, Aspect, Near, Far);
}
```

**핵심 포인트**:

1. **Light Position**: DirectionalLight와 달리 계산 불필요, Component의 WorldLocation 직접 사용
2. **Target 계산**: `LightPos + LightDir` (간단한 방향 벡터 더하기)
3. **FOV 계산**: `OuterConeAngle × 2`
   - OuterConeAngle은 cone의 반각 (half-angle)
   - 전체 cone을 커버하려면 2배 필요
4. **Near Plane**: 1.0 고정 (너무 작으면 depth precision 문제)
5. **Far Plane**: AttenuationRadius (light가 닿는 최대 거리)
6. **Aspect 1.0**: 정사각형 shadow map 사용

**DirectionalLight와 비교**:
```cpp
// DirectionalLight (복잡)
FVector SceneCenter = (MinBounds + MaxBounds) * 0.5f;
float SceneRadius = (MaxBounds - MinBounds).Length() * 0.5f;
FVector LightPos = SceneCenter - LightDir * (SceneRadius + 50.0f);
OutProj = CreateOrthoLH(...);  // AABB 기반 계산

// SpotLight (간단)
FVector LightPos = Light->GetWorldLocation();  // 그대로 사용!
FVector Target = LightPos + LightDir;
OutProj = CreatePerspectiveFovLH(FovY, ...);  // FOV 기반
```

---

### Phase 3: Per-Light Rasterizer State 캐싱

**목적**: SpotLight별 DepthBias를 가진 rasterizer state 관리

#### 3-1. GetOrCreateRasterizerState 오버로드

**헤더 파일**: `Engine/Source/Render/RenderPass/Public/ShadowMapPass.h`

```cpp
class FShadowMapPass : public FRenderPass
{
private:
    // DirectionalLight용 (기존)
    TMap<UDirectionalLightComponent*, ID3D11RasterizerState*> DirectionalRasterizerStates;
    ID3D11RasterizerState* GetOrCreateRasterizerState(UDirectionalLightComponent* Light);

    // SpotLight용 (새로 추가)
    TMap<USpotLightComponent*, ID3D11RasterizerState*> SpotRasterizerStates;
    ID3D11RasterizerState* GetOrCreateRasterizerState(USpotLightComponent* Light);
};
```

**구현**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
ID3D11RasterizerState* FShadowMapPass::GetOrCreateRasterizerState(USpotLightComponent* Light)
{
    // 이미 생성된 state가 있으면 재사용
    auto It = SpotRasterizerStates.find(Light);
    if (It != SpotRasterizerStates.end())
        return It->second;

    // 새로 생성
    const auto& Renderer = URenderer::GetInstance();
    D3D11_RASTERIZER_DESC RastDesc = {};
    ShadowRasterizerState->GetDesc(&RastDesc);

    // Light별 DepthBias 설정
    // DepthBias: Shadow acne (자기 그림자 아티팩트) 방지
    //   - 공식: FinalDepth = OriginalDepth + DepthBias*r + SlopeScaledDepthBias*MaxSlope
    //   - r: Depth buffer의 최소 표현 단위 (format dependent)
    //   - MaxSlope: max(|dz/dx|, |dz/dy|) - 표면의 기울기
    //   - 100000.0f: float → integer 변환 스케일
    RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
    RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();

    ID3D11RasterizerState* NewState = nullptr;
    Renderer.GetDevice()->CreateRasterizerState(&RastDesc, &NewState);

    // 캐시에 저장
    SpotRasterizerStates[Light] = NewState;

    return NewState;
}
```

**Cleanup 코드**: `Release()` 함수

```cpp
void FShadowMapPass::Release()
{
    // ... DirectionalLight cleanup ...

    // SpotLight rasterizer states 해제
    for (auto& Pair : SpotRasterizerStates)
    {
        if (Pair.second)
            Pair.second->Release();
    }
    SpotRasterizerStates.clear();

    // ... 다른 리소스 cleanup ...
}
```

---

### Phase 4: Spot Shadow Map 렌더링

**목적**: SpotLight 시점에서 depth 렌더링

#### 4-1. RenderSpotShadowMap 구현

**파일**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
void FShadowMapPass::RenderSpotShadowMap(USpotLightComponent* Light,
    const TArray<UStaticMeshComponent*>& Meshes)
{
    FShadowMapResource* ShadowMap = GetOrCreateShadowMap(Light);
    if (!ShadowMap || !ShadowMap->IsValid())
        return;

    const auto& Renderer = URenderer::GetInstance();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();

    // 0. 현재 상태 저장 (복원용)
    ID3D11RenderTargetView* OriginalRTV = nullptr;
    ID3D11DepthStencilView* OriginalDSV = nullptr;
    DeviceContext->OMGetRenderTargets(1, &OriginalRTV, &OriginalDSV);

    D3D11_VIEWPORT OriginalViewport;
    UINT NumViewports = 1;
    DeviceContext->RSGetViewports(&NumViewports, &OriginalViewport);

    // 1. Shadow render target 설정
    // Note: RenderTargets는 Pipeline API 사용, Viewport는 Pipeline 미지원으로 DeviceContext 직접 사용
    ID3D11RenderTargetView* NullRTV = nullptr;
    Pipeline->SetRenderTargets(1, &NullRTV, ShadowMap->ShadowDSV.Get());
    DeviceContext->RSSetViewports(1, &ShadowMap->ShadowViewport);
    DeviceContext->ClearDepthStencilView(ShadowMap->ShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 2. Light별 캐싱된 rasterizer state 가져오기 (DepthBias 포함)
    ID3D11RasterizerState* RastState = GetOrCreateRasterizerState(Light);

    // 3. Pipeline을 통해 shadow rendering state 설정
    FPipelineInfo ShadowPipelineInfo = {
        DepthOnlyInputLayout,
        DepthOnlyShader,
        RastState,  // 캐싱된 state 사용 (매 프레임 생성/해제 방지)
        ShadowDepthStencilState,
        nullptr,  // No pixel shader for depth-only
        nullptr,  // No blend state
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };
    Pipeline->UpdatePipeline(ShadowPipelineInfo);

    // 4. Light view-projection 계산 (Perspective projection for cone-shaped frustum)
    FMatrix LightView, LightProj;
    CalculateSpotLightViewProj(Light, Meshes, LightView, LightProj);

    // Store the calculated shadow view-projection matrix in the light component
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
    // RenderTarget과 DepthStencil 복원 (Pipeline API 사용)
    Pipeline->SetRenderTargets(1, &OriginalRTV, OriginalDSV);

    // Viewport 복원 (DeviceContext 직접 사용)
    DeviceContext->RSSetViewports(1, &OriginalViewport);

    // 임시 리소스 해제
    if (OriginalRTV)
        OriginalRTV->Release();
    if (OriginalDSV)
        OriginalDSV->Release();

    // Note: RastState는 캐싱되므로 여기서 해제하지 않음 (Release()에서 일괄 해제)
}
```

**핵심 포인트**:
- DirectionalLight의 `RenderDirectionalShadowMap`과 거의 동일한 구조
- 차이점: `CalculateSpotLightViewProj()` 호출 (Perspective projection)

#### 4-2. Execute()에서 SpotLight 처리 추가

```cpp
void FShadowMapPass::Execute(FRenderingContext& Context)
{
    // IMPORTANT: Unbind shadow map SRVs before rendering to them as DSV
    // This prevents D3D11 resource hazard warnings
    const auto& Renderer = URenderer::GetInstance();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    DeviceContext->PSSetShaderResources(10, 2, NullSRVs);  // Unbind t10-t11 (DirectionalShadowMap, SpotShadowMap)

    // Phase 1: Directional Lights
    for (auto DirLight : Context.DirectionalLights)
    {
        if (DirLight->GetCastShadows() && DirLight->GetLightEnabled())
        {
            RenderDirectionalShadowMap(DirLight, Context.StaticMeshes);
        }
    }

    // Phase 2: Spot Lights (새로 추가)
    for (auto SpotLight : Context.SpotLights)
    {
        if (SpotLight->GetCastShadows() && SpotLight->GetLightEnabled())
        {
            RenderSpotShadowMap(SpotLight, Context.StaticMeshes);
        }
    }

    // Phase 3: Point Lights
    // TODO: Implement point light cube shadow mapping
}
```

---

### Phase 5: GetSpotShadowMap 접근자

**목적**: StaticMeshPass에서 shadow map을 가져올 수 있도록

**헤더**: `Engine/Source/Render/RenderPass/Public/ShadowMapPass.h`

```cpp
class FShadowMapPass : public FRenderPass
{
public:
    FShadowMapResource* GetSpotShadowMap(USpotLightComponent* Light);

private:
    TMap<USpotLightComponent*, FShadowMapResource*> SpotShadowMaps;
};
```

**구현**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
FShadowMapResource* FShadowMapPass::GetSpotShadowMap(USpotLightComponent* Light)
{
    auto It = SpotShadowMaps.find(Light);
    return It != SpotShadowMaps.end() ? It->second : nullptr;
}
```

**GetOrCreateShadowMap 수정** (SpotLight 지원):

```cpp
FShadowMapResource* FShadowMapPass::GetOrCreateShadowMap(ULightComponent* Light)
{
    FShadowMapResource* ShadowMap = nullptr;
    ID3D11Device* Device = URenderer::GetInstance().GetDevice();

    if (auto DirLight = Cast<UDirectionalLightComponent>(Light))
    {
        // ... DirectionalLight 처리 ...
    }
    else if (auto SpotLight = Cast<USpotLightComponent>(Light))
    {
        auto It = SpotShadowMaps.find(SpotLight);
        if (It == SpotShadowMaps.end())
        {
            // 새로 생성
            ShadowMap = new FShadowMapResource();
            ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
            SpotShadowMaps[SpotLight] = ShadowMap;
        }
        else
        {
            ShadowMap = It->second;
            // 해상도 변경 체크
            if (ShadowMap->NeedsResize(Light->GetShadowMapResolution()))
            {
                ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
            }
        }
    }

    return ShadowMap;
}
```

---

### Phase 6: SpotLightComponent에 Shadow Matrix 캐싱

**목적**: GPU로 전송할 LightViewProjection matrix 저장

**헤더**: `Engine/Source/Component/Public/SpotLightComponent.h`

```cpp
class USpotLightComponent : public UPointLightComponent
{
public:
    // Shadow mapping
    void SetShadowViewProjection(const FMatrix& ViewProj) { CachedShadowViewProjection = ViewProj; }
    const FMatrix& GetShadowViewProjection() const { return CachedShadowViewProjection; }

private:
    // Shadow mapping
    FMatrix CachedShadowViewProjection = FMatrix::Identity();
};
```

**GetSpotLightInfo 수정**: `Engine/Source/Component/Private/SpotLightComponent.cpp`

```cpp
FSpotLightInfo USpotLightComponent::GetSpotLightInfo() const
{
    FSpotLightInfo Info;
    Info.Color = FVector4(LightColor, 1);
    Info.Position = GetWorldLocation();
    Info.Intensity = Intensity;
    Info.Range = AttenuationRadius;
    Info.DistanceFalloffExponent = DistanceFalloffExponent;
    Info.InnerConeAngle = InnerConeAngleRad;
    Info.OuterConeAngle = OuterConeAngleRad;
    Info.AngleFalloffExponent = AngleFalloffExponent;
    Info.Direction = GetForwardVector();

    // Shadow parameters
    Info.LightViewProjection = CachedShadowViewProjection; // Updated by ShadowMapPass
    Info.CastShadow = GetCastShadows() ? 1u : 0u;
    Info.ShadowBias = GetShadowBias();
    Info.ShadowSlopeBias = GetShadowSlopeBias();
    Info.ShadowSharpen = GetShadowSharpen();

    return Info;
}
```

---

### Phase 7: Shader Shadow Sampling

**목적**: Camera 시점에서 SpotLight shadow map 샘플링

#### 7-1. Shadow Map Texture 선언

**파일**: `Engine/Asset/Shader/UberLit.hlsl`

```hlsl
// Shadow map resources
Texture2D DirectionalShadowMap : register(t10);
Texture2D SpotShadowMap : register(t11);  // 추가
SamplerComparisonState ShadowSampler : register(s1);
```

#### 7-2. CalculateSpotShadowFactor 구현

**파일**: `Engine/Asset/Shader/UberLit.hlsl`

```hlsl
float CalculateSpotShadowFactor(FSpotLightInfo Light, float3 WorldPos)
{
    // If shadow is disabled, return fully lit (1.0)
    if (Light.CastShadow == 0)
        return 1.0f;

    // Transform world position to light space
    float4 LightSpacePos = mul(float4(WorldPos, 1.0f), Light.LightViewProjection);

    // Perspective divide (to NDC space)
    // SpotLight는 Perspective projection이므로 w로 나누기 필수!
    LightSpacePos.xyz /= LightSpacePos.w;

    // Check if position is outside light frustum
    if (LightSpacePos.x < -1.0f || LightSpacePos.x > 1.0f ||
        LightSpacePos.y < -1.0f || LightSpacePos.y > 1.0f ||
        LightSpacePos.z < 0.0f || LightSpacePos.z > 1.0f)
    {
        return 1.0f; // Outside shadow map, assume lit
    }

    // Convert NDC to texture coordinates ([-1,1] -> [0,1])
    float2 ShadowTexCoord;
    ShadowTexCoord.x = LightSpacePos.x * 0.5f + 0.5f;
    ShadowTexCoord.y = -LightSpacePos.y * 0.5f + 0.5f; // Flip Y for texture space

    // Current pixel depth in light space
    float CurrentDepth = LightSpacePos.z;

    // PCF (Percentage Closer Filtering) for soft shadows
    float ShadowFactor = 0.0f;
    float2 TexelSize = 1.0f / float2(1024.0f, 1024.0f); // Spot shadow map resolution

    // 3x3 PCF kernel
    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            float2 Offset = float2(x, y) * TexelSize;
            ShadowFactor += SpotShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                ShadowTexCoord + Offset,
                CurrentDepth
            );
        }
    }

    ShadowFactor /= 9.0f; // Average of 9 samples

    return ShadowFactor;
}
```

**핵심 차이점** (DirectionalLight와 비교):
- **Perspective Divide 필수**: `LightSpacePos.xyz /= LightSpacePos.w`
  - DirectionalLight (Orthographic): w가 항상 1.0이므로 divide해도 변화 없음
  - SpotLight (Perspective): w가 depth에 따라 변하므로 divide 필수!

#### 7-3. CalculateSpotLight에 Shadow 적용

**파일**: `Engine/Asset/Shader/UberLit.hlsl`

```hlsl
FIllumination CalculateSpotLight(FSpotLightInfo Info, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    FIllumination Result = (FIllumination) 0;

    // ... 기존 lighting 계산 ...

    // Shadow factor 계산 (추가)
    float ShadowFactor = CalculateSpotShadowFactor(Info, WorldPos);

    // Shadow 적용 (기존 코드에 ShadowFactor 곱하기)
    Result.Diffuse = Info.Color * Info.Intensity * NdotL * AttenuationDistance * AttenuationAngle * ShadowFactor;
    Result.Specular = Info.Color * Info.Intensity * Spec * AttenuationDistance * AttenuationAngle * ShadowFactor;

    return Result;
}
```

---

### Phase 8: StaticMeshPass에서 Shadow Map Binding

**목적**: SpotLight shadow map을 shader에 전달

**파일**: `Engine/Source/Render/RenderPass/Private/StaticMeshPass.cpp`

#### 8-1. Include 추가

```cpp
#include "Component/Public/SpotLightComponent.h"  // 추가
```

#### 8-2. Execute()에서 SpotShadowMap Binding

```cpp
void FStaticMeshPass::Execute(FRenderingContext& Context)
{
    // ... pipeline setup ...

    // Bind shadow comparison sampler
    Pipeline->SetSamplerState(1, EShaderType::PS, Renderer.GetShadowComparisonSampler());

    // Bind directional light shadow map if available
    if (!Context.DirectionalLights.empty() && Context.DirectionalLights[0]->GetCastShadows())
    {
        // ... DirectionalLight shadow binding ...
    }

    // Bind spot light shadow map if available (추가)
    if (!Context.SpotLights.empty() && Context.SpotLights[0]->GetCastShadows())
    {
        USpotLightComponent* SpotLight = Context.SpotLights[0];
        FShadowMapPass* ShadowPass = Renderer.GetShadowMapPass();
        if (ShadowPass)
        {
            FShadowMapResource* ShadowMap = ShadowPass->GetSpotShadowMap(SpotLight);
            if (ShadowMap && ShadowMap->IsValid())
            {
                Pipeline->SetShaderResourceView(11, EShaderType::PS, ShadowMap->ShadowSRV.Get());
            }
        }
    }

    // ... mesh rendering ...
}
```

---

### Phase 9: Resource Hazard 방지

**목적**: Shadow map을 DSV(쓰기)와 SRV(읽기)로 동시에 바인딩하는 것 방지

**파일**: `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`

```cpp
void FShadowMapPass::Execute(FRenderingContext& Context)
{
    // IMPORTANT: Unbind shadow map SRVs before rendering to them as DSV
    // This prevents D3D11 resource hazard warnings
    const auto& Renderer = URenderer::GetInstance();
    ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
    ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
    DeviceContext->PSSetShaderResources(10, 2, NullSRVs);  // Unbind t10-t11

    // ... shadow rendering ...
}
```

**이유**:
```
StaticMeshPass에서:
  t10 = DirectionalShadowMap (SRV로 읽기)
  t11 = SpotShadowMap (SRV로 읽기)
         ↓
ShadowMapPass에서:
  DSV = DirectionalShadowMap (쓰기) ← Conflict!
  DSV = SpotShadowMap (쓰기) ← Conflict!
         ↓
  해결: SRV unbind 후 DSV 사용
```

---

## 핵심 코드 설명

### Perspective vs Orthographic

**Orthographic Projection** (DirectionalLight):
```
Z = (depth - Near) / (Far - Near)  // Linear depth
w = 1.0 (always)
```

**Perspective Projection** (SpotLight):
```
x' = x × (f / aspect) / z
y' = y × f / z
z' = (z × Far) / (Far - Near) - (Near × Far) / (Far - Near)
w' = z  // ← 중요: w가 depth에 따라 변함!
```

**NDC 변환 시**:
```hlsl
// Perspective divide
LightSpacePos.xyz /= LightSpacePos.w;  // 필수!

// 이제 xyz는 NDC 좌표 ([-1,1] for x,y; [0,1] for z)
```

### FOV 계산 원리

```
OuterConeAngle = 45도 (PI/4)
       ↓
Full Cone FOV = 45 × 2 = 90도

왜 2배?
    *  ← Light
   /|\   ← OuterConeAngle (한쪽)
  / | \
 /  |  \  ← Full cone은 양쪽
```

### Near/Far Plane 선택

**Near = 1.0**:
- 너무 작으면 (0.01): Depth precision 문제 (Z-fighting)
- 너무 크면 (10.0): Light 근처 객체가 shadow 범위 밖

**Far = AttenuationRadius**:
- Light가 닿는 최대 거리
- 이 거리 밖은 어차피 조명 안 받음

---

## 동작 원리

### 전체 렌더링 파이프라인

```
1. ShadowMapPass::Execute()
   ├─ SpotLights 순회
   ├─ RenderSpotShadowMap()
   │  ├─ CalculateSpotLightViewProj()
   │  │  ├─ Light position 가져오기 (GetWorldLocation)
   │  │  ├─ Light direction 가져오기 (GetForwardVector)
   │  │  ├─ Light View Matrix 생성 (CreateLookAtLH)
   │  │  └─ Perspective Projection 생성 (CreatePerspectiveFovLH)
   │  │     ├─ FOV = OuterConeAngle × 2
   │  │     ├─ Aspect = 1.0
   │  │     ├─ Near = 1.0
   │  │     └─ Far = AttenuationRadius
   │  ├─ Shadow Map 리소스 준비
   │  ├─ Pipeline state 설정 (DepthOnly shader, DepthBias)
   │  ├─ Light에 ViewProj matrix 저장
   │  └─ 각 Mesh depth 렌더링
   └─ Shadow Map 생성 완료

2. StaticMeshPass::Execute()
   ├─ SpotShadowMap SRV (t11)와 Comparison Sampler Binding
   ├─ Light info (LightViewProjection 포함) GPU 전송
   └─ UberLit shader 실행
      ├─ CalculateSpotShadowFactor()
      │  ├─ World → Light space 변환
      │  ├─ Perspective divide (w로 나누기)
      │  ├─ Frustum culling
      │  ├─ NDC → Texture coordinate 변환
      │  └─ 3×3 PCF sampling
      └─ Shadow factor를 lighting에 곱함
```

### DirectionalLight vs SpotLight 렌더링 비교

| 단계 | Directional Light | Spot Light |
|------|-------------------|------------|
| **Light Position** | Scene AABB 기반 계산 | GetWorldLocation() 직접 사용 |
| **View Matrix** | CreateLookAtLH(계산된 pos, scene center, up) | CreateLookAtLH(light pos, light pos + dir, up) |
| **Projection** | CreateOrthoLH(AABB bounds) | CreatePerspectiveFovLH(cone angle × 2) |
| **Shadow Map** | 2048×2048 | 1024×1024 |
| **Shader** | DirectionalShadowMap (t10) | SpotShadowMap (t11) |
| **Perspective Divide** | 선택 (w=1.0) | **필수 (w=depth)** |

---

## 트러블슈팅

### 문제 1: SpotLight 회전 시 그림자가 바뀌지 않는다고 생각함

**증상**: SpotLight를 회전해도 그림자가 원래 spawn 방향 그대로

**원인**: 잘못된 이해! Unreal Engine도 동일하게 동작

**해결**: 정상 동작. SpotLight shadow는 회전에 따라 실시간으로 업데이트됨

**확인 방법**:
1. Unreal Engine에서 SpotLight 생성
2. Shadow 활성화
3. 회전 → 그림자가 함께 회전함 확인

---

### 문제 2: 그림자가 보이지 않음

**증상**: SpotLight shadow map pass는 실행되지만 그림자가 렌더링 안됨

**가능한 원인**:

1. **Shadow Map이 shader에 바인딩 안됨**:
```cpp
// StaticMeshPass.cpp 확인
Pipeline->SetShaderResourceView(11, EShaderType::PS, ShadowMap->ShadowSRV.Get());
```

2. **LightViewProjection이 Identity**:
```cpp
// SpotLightComponent.cpp 확인
Info.LightViewProjection = CachedShadowViewProjection;  // Identity가 아니어야 함
```

3. **Perspective divide 누락**:
```hlsl
// UberLit.hlsl 확인
LightSpacePos.xyz /= LightSpacePos.w;  // 필수!
```

---

### 문제 3: 그림자가 이상한 모양

**증상**: 그림자가 찌그러지거나 왜곡됨

**원인 1: FOV가 너무 큼**
```cpp
// OuterConeAngle이 90도 이상이면 FOV가 180도 이상
// 해결: Clamp
FovY = std::clamp(FovY, 0.1f, PI - 0.1f);
```

**원인 2: Aspect ratio 잘못됨**
```cpp
// SpotLight는 정사각형 shadow map 사용
float Aspect = 1.0f;  // 고정값 사용
```

---

### 문제 4: Far Plane이 너무 작음

**증상**: 멀리 있는 객체는 그림자가 안 생김

**원인**: `Far = AttenuationRadius`가 너무 작음

**해결**:
1. SpotLight의 AttenuationRadius 증가
2. 또는 Far plane 계산 수정:
```cpp
float Far = Light->GetAttenuationRadius() * 1.5f;  // 여유 추가
```

---

## 성능 최적화

### 최적화 1: Shadow Map Resolution

**Trade-off**:
- 높은 해상도 (2048×2048): 선명한 그림자, 메모리↑, 느림
- 낮은 해상도 (512×512): 흐릿한 그림자, 메모리↓, 빠름

**권장**:
- SpotLight: 1024×1024 (기본값)
- DirectionalLight: 2048×2048 (더 넓은 영역 커버)

**이유**: SpotLight는 local light이므로 화면에서 차지하는 영역이 작음

---

### 최적화 2: PCF Kernel Size

**현재**: 3×3 (9 samples per pixel)

**대안**:
- 2×2 (4 samples): 빠름, 약간 거친 edge
- 5×5 (25 samples): 매우 부드러움, 2.7배 느림

**권장**: 3×3 유지 (품질/성능 균형)

---

### 최적화 3: Frustum Culling

**현재**: 모든 mesh를 shadow map에 렌더링

**개선**:
```cpp
// CalculateSpotLightViewProj에서 frustum 계산
FFrustum LightFrustum = CreateFrustum(LightView, LightProj);

// RenderSpotShadowMap에서 culling
for (auto Mesh : Meshes)
{
    if (Mesh->IsVisible() && LightFrustum.Intersects(Mesh->GetBounds()))
    {
        RenderMeshDepth(Mesh, LightView, LightProj);
    }
}
```

**성능 향상**: SpotLight cone 밖 객체 렌더링 생략

---

### 최적화 4: LOD (Level of Detail)

**아이디어**: Shadow map에는 간단한 mesh 사용

```cpp
// RenderMeshDepth에서
UStaticMesh* ShadowLOD = Mesh->GetShadowLOD();  // 낮은 폴리곤 버전
// Use ShadowLOD instead of full detail mesh
```

**성능 향상**: Triangle count 감소

---

## 테스트 방법

### 1. 기본 SpotLight Shadow 테스트

1. Scene에 **SpotLight** 추가
2. Light의 **Cast Shadows** 활성화
3. **OuterConeAngle** 조정 (예: 45도)
4. **AttenuationRadius** 조정 (예: 500)
5. Ground plane과 몇 개의 cube 배치
6. **Rotation**으로 조명 방향 변경
7. **Play** 실행
8. 원뿔형 조명과 그림자 확인

### 2. Shadow 파라미터 조정

**Shadow Acne** (떨림 현상) 발생 시:
- `ShadowBias` 증가 (0.001 → 0.002)
- `ShadowSlopeBias` 증가 (1.0 → 2.0)

**Peter Panning** (그림자가 물체에서 떨어짐) 발생 시:
- `ShadowBias` 감소
- `ShadowSlopeBias` 감소

### 3. FOV 테스트

**OuterConeAngle 변경**:
- 15도: 좁은 spotlight (손전등)
- 45도: 중간 (무대 조명)
- 60도: 넓은 cone
- 89도: 거의 반구 (최대값)

**확인**: Shadow map frustum이 cone 모양과 일치하는지

### 4. Attenuation 테스트

**AttenuationRadius 변경**:
- 100: 짧은 범위
- 500: 중간 범위
- 1000: 긴 범위

**확인**:
- Light가 닿지 않는 곳은 그림자도 없음
- Shadow map far plane이 적절한지

### 5. 다중 SpotLight

**현재 제한**: 첫 번째 SpotLight만 shadow casting

**확장 방법**:
```cpp
// StaticMeshPass.cpp
for (int i = 0; i < min(Context.SpotLights.size(), MaxSpotShadows); i++)
{
    auto SpotLight = Context.SpotLights[i];
    if (SpotLight->GetCastShadows())
    {
        // Bind to t11+i, t12, t13...
        Pipeline->SetShaderResourceView(11 + i, EShaderType::PS, ...);
    }
}
```

```hlsl
// UberLit.hlsl
Texture2D SpotShadowMaps[MAX_SPOT_SHADOWS] : register(t11);
```

---

## 참고 자료

### 관련 파일

**Shadow Map Pass**:
- [Engine/Source/Render/RenderPass/Public/ShadowMapPass.h](Engine/Source/Render/RenderPass/Public/ShadowMapPass.h)
- [Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp](Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp)

**Shadow Map 리소스**:
- [Engine/Source/Texture/Public/ShadowMapResources.h](Engine/Source/Texture/Public/ShadowMapResources.h)
- [Engine/Source/Texture/Private/ShadowMapResources.cpp](Engine/Source/Texture/Private/ShadowMapResources.cpp)

**셰이더**:
- [Engine/Asset/Shader/DepthOnlyVS.hlsl](Engine/Asset/Shader/DepthOnlyVS.hlsl) - Depth-only vertex shader (DirectionalLight와 공유)
- [Engine/Asset/Shader/UberLit.hlsl](Engine/Asset/Shader/UberLit.hlsl) - SpotLight shadow sampling shader

**Light Component**:
- [Engine/Source/Component/Public/SpotLightComponent.h](Engine/Source/Component/Public/SpotLightComponent.h)
- [Engine/Source/Component/Private/SpotLightComponent.cpp](Engine/Source/Component/Private/SpotLightComponent.cpp)

**Rendering**:
- [Engine/Source/Render/RenderPass/Private/StaticMeshPass.cpp](Engine/Source/Render/RenderPass/Private/StaticMeshPass.cpp) - Shadow map binding
- [Engine/Source/Render/Renderer/Private/Renderer.cpp](Engine/Source/Render/Renderer/Private/Renderer.cpp) - Comparison sampler (DirectionalLight와 공유)

**Core Types**:
- [Engine/Source/Global/CoreTypes.h](Engine/Source/Global/CoreTypes.h) - `FSpotLightInfo` 구조체

### 외부 참고 자료

- **Microsoft DirectX 11 Documentation**:
  - Shadow Mapping: https://docs.microsoft.com/en-us/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps
  - Perspective Projection: https://docs.microsoft.com/en-us/windows/win32/direct3d9/projection-transform

- **Real-Time Rendering (4th Edition)**:
  - Chapter 7: Shadows
  - Section 7.4: Spot Light Shadows

- **GPU Gems**:
  - Chapter 11: Shadow Map Antialiasing
  - Chapter 17: Efficient Soft-Edged Shadows Using Pixel Shader Branching

---

## 구현 체크리스트

- [x] **Phase 1**: Perspective Projection 헬퍼
  - [x] CreatePerspectiveFovLH 구현

- [x] **Phase 2**: Spot Light View-Projection 계산
  - [x] CalculateSpotLightViewProj 구현
  - [x] FOV = OuterConeAngle × 2
  - [x] Near/Far plane 설정

- [x] **Phase 3**: Per-Light Rasterizer State
  - [x] GetOrCreateRasterizerState(SpotLightComponent*) 오버로드
  - [x] SpotRasterizerStates 맵 추가
  - [x] Release()에서 cleanup

- [x] **Phase 4**: Shadow Map 렌더링
  - [x] RenderSpotShadowMap 구현
  - [x] Execute()에서 SpotLight 처리
  - [x] GetOrCreateShadowMap SpotLight 지원

- [x] **Phase 5**: Shadow Map 접근자
  - [x] GetSpotShadowMap 구현

- [x] **Phase 6**: Shadow Matrix 캐싱
  - [x] SpotLightComponent::SetShadowViewProjection
  - [x] SpotLightComponent::GetShadowViewProjection
  - [x] GetSpotLightInfo에서 matrix 전달

- [x] **Phase 7**: Shader Shadow Sampling
  - [x] SpotShadowMap texture 선언 (t11)
  - [x] CalculateSpotShadowFactor 구현
  - [x] Perspective divide 추가
  - [x] CalculateSpotLight에 shadow 적용

- [x] **Phase 8**: StaticMeshPass Binding
  - [x] SpotLightComponent.h include
  - [x] SpotShadowMap SRV binding (t11)

- [x] **Phase 9**: Resource Hazard 방지
  - [x] Execute()에서 t10-t11 unbind

- [x] **최적화**
  - [x] Rasterizer state 캐싱
  - [x] Shadow map 리소스 재사용
  - [x] Resource hazard 방지

---

## DirectionalLight 대비 주요 차이점 요약

| 항목 | DirectionalLight | SpotLight |
|------|------------------|-----------|
| **Projection Type** | Orthographic | **Perspective** |
| **Matrix 계산 복잡도** | 높음 (AABB 필요) | **낮음 (position + direction)** |
| **FOV** | N/A | **OuterConeAngle × 2** |
| **Light Position** | Scene 기반 계산 | **WorldLocation 직접 사용** |
| **AABB 계산** | 필수 | **불필요** |
| **Perspective Divide** | 선택 (w=1.0) | **필수 (w=depth)** |
| **Shadow Map 해상도** | 2048×2048 | **1024×1024** |
| **적용 범위** | Global (전체 씬) | **Local (cone 내부만)** |

---

## 향후 개선 사항

### 1. 다중 SpotLight Shadow

**목적**: 여러 SpotLight의 shadow 동시 지원

**방법**: Texture array 사용
```hlsl
Texture2D SpotShadowMaps[MAX_SPOT_SHADOWS] : register(t11);
```

### 2. Soft Shadows (PCSS)

**목적**: 거리에 따른 자연스러운 soft shadow

**방법**: Percentage Closer Soft Shadows (PCSS) 구현

### 3. Shadow Map Pooling

**목적**: 메모리 효율 개선

**방법**: 고정 크기 atlas에 여러 shadow map 배치

### 4. Temporal Shadow Filtering

**목적**: Shadow edge의 temporal aliasing 감소

**방법**: 이전 프레임 shadow와 blend

---

**문서 끝**

이 문서는 FutureEngine의 Spot Light Shadow Mapping 구현을 완전히 설명합니다. 질문이나 추가 설명이 필요하면 Week8 Team5에 문의하세요.
