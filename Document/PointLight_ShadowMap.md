# Point Light Shadow Mapping 구현 가이드

**작성일**: 2025-10-25
**버전**: 1.0
**구현자**: Week8 Team5

---

## 목차

1. [개요](#개요)
2. [Cube Shadow Mapping 원리](#cube-shadow-mapping-원리)
3. [DirectionalLight/SpotLight와의 차이점](#directionallight-spotlight와의-차이점)
4. [구현 단계](#구현-단계)
5. [핵심 코드 설명](#핵심-코드-설명)
6. [Linear Distance 방식](#linear-distance-방식)
7. [Soft Shadows (PCF)](#soft-shadows-pcf)
8. [트러블슈팅](#트러블슈팅)
9. [성능 최적화](#성능-최적화)
10. [테스트 방법](#테스트-방법)
11. [참고 자료](#참고-자료)

---

## 개요

### 목적

FutureEngine에 **Point Light용 Cube Shadow Mapping**을 구현하여 전구, 횃불과 같은 점광원에서 **전방향(omnidirectional) 그림자**를 렌더링합니다.

### 주요 특징

- **Cube Shadow Map**: 6개 면을 사용한 전방향 shadow
- **Linear Distance 저장**: Perspective depth 대신 euclidean distance 사용
- **TextureCube 샘플링**: 3D direction vector로 shadow map 샘플링
- **Manual Depth Comparison**: TextureCube는 SampleCmp 미지원
- **PCF (21 samples)**: 3D 오프셋을 사용한 부드러운 그림자
- **512×512 해상도** (6면): 균형잡힌 품질과 성능
- **Rasterizer State 캐싱**: 매 프레임 GPU 리소스 생성 방지
- **Per-Light Cube Map**: 각 PointLight마다 독립적인 cube shadow map

### 기술 스펙

| 항목 | 값 |
|------|-----|
| Shadow Map 해상도 | 512×512 × 6 faces (기본값) |
| Depth Format | DXGI_FORMAT_R32_TYPELESS (32-bit float) |
| DSV Format | DXGI_FORMAT_D32_FLOAT |
| SRV Format | DXGI_FORMAT_R32_FLOAT |
| Texture Type | **TextureCube** (6 array slices) |
| Projection Type | Perspective × 6 (Left-Handed) |
| FOV | **90도 고정** (cube face) |
| Aspect Ratio | 1.0 (정사각형) |
| Near Plane | 1.0 |
| Far Plane | AttenuationRadius |
| Depth Storage | **Linear Distance / Range** |
| PCF Samples | **21 samples** (3D offsets) |
| Default Bias | 0.005f |
| Default Filter Radius | 0.01f (softness) |

---

## Cube Shadow Mapping 원리

### Omnidirectional Shadow

Point Light는 모든 방향으로 빛을 발산하므로, **6개의 면을 가진 Cube Map**이 필요합니다:

```
        +Y (Top)
         |
-X ---- Center ---- +X
         |
        -Y (Bottom)

        +Z (Front)
        -Z (Back)
```

### 6-Face Rendering

각 cube face는 **90도 FOV perspective projection**으로 렌더링됩니다:

```
Face 0: +X direction (Right)   - LookAt(LightPos, LightPos + (1,0,0), (0,1,0))
Face 1: -X direction (Left)    - LookAt(LightPos, LightPos + (-1,0,0), (0,1,0))
Face 2: +Y direction (Top)     - LookAt(LightPos, LightPos + (0,1,0), (0,0,-1))
Face 3: -Y direction (Bottom)  - LookAt(LightPos, LightPos + (0,-1,0), (0,0,1))
Face 4: +Z direction (Front)   - LookAt(LightPos, LightPos + (0,0,1), (0,1,0))
Face 5: -Z direction (Back)    - LookAt(LightPos, LightPos + (0,0,-1), (0,1,0))
```

**주의**: Y축 방향 face는 up vector가 다릅니다 (Gimbal lock 방지).

### Linear Distance 방식

**왜 Linear Distance인가?**

Perspective projection depth는 **view space Z**를 기반으로 합니다. 하지만 cube map의 각 face는 서로 다른 view space를 가지므로, **world space euclidean distance**를 사용하는 것이 일관된 비교를 가능하게 합니다.

```
Perspective Depth (잘못된 방식):
  depth = (far / (far - near)) * (1 - near / viewZ)
  → 각 face마다 viewZ가 다름
  → 6개 spotlight처럼 보이는 artifact 발생

Linear Distance (올바른 방식):
  distance = length(WorldPos - LightPos)
  depth = saturate(distance / range)
  → 모든 방향에서 일관됨
  → 부드러운 omnidirectional shadow
```

### TextureCube 샘플링

일반 2D shadow map과 달리, cube map은 **3D direction vector**로 샘플링합니다:

```hlsl
// 2D Shadow Map (Directional/Spot Light)
float2 UV = ConvertToTexCoord(LightSpacePos.xy);
float depth = ShadowMap.SampleCmp(sampler, UV, currentDepth);

// Cube Shadow Map (Point Light)
float3 direction = WorldPos - LightPos;
float depth = PointShadowMap.SampleLevel(sampler, direction, 0).r;
// SampleCmp 사용 불가! Manual comparison 필요
```

---

## DirectionalLight/SpotLight와의 차이점

### 비교표

| 항목 | Directional Light | Spot Light | **Point Light** |
|------|------------------|-----------|----------------|
| **Shadow Map 타입** | Texture2D | Texture2D | **TextureCube** |
| **Projection** | Orthographic | Perspective | **Perspective × 6** |
| **FOV** | N/A (orthographic) | OuterCone × 2 | **90° 고정** |
| **렌더링 횟수** | 1회 | 1회 | **6회** |
| **DSV 개수** | 1개 | 1개 | **6개** |
| **SRV 개수** | 1개 | 1개 | **1개 (cube)** |
| **Depth 저장** | Perspective depth | Perspective depth | **Linear distance** |
| **샘플링 방식** | UV coordinates | UV coordinates | **3D direction** |
| **HW PCF** | SampleCmp 가능 | SampleCmp 가능 | **불가능** |
| **Shadow 비교** | GPU 자동 | GPU 자동 | **Manual (shader)** |
| **AABB 계산** | 필요 (scene bounds) | 불필요 | **불필요** |
| **View Frustum** | 수동 계산 | Cone frustum | **Sphere** |
| **기본 해상도** | 2048×2048 | 1024×1024 | **512×512 × 6** |

### 메모리 사용량

- **Directional**: 2048×2048 × 4 bytes = **16 MB**
- **Spot**: 1024×1024 × 4 bytes = **4 MB**
- **Point**: 512×512 × 6 faces × 4 bytes = **6 MB**

---

## 구현 단계

### Phase 1: CalculatePointLightViewProj (6개 View-Projection 계산)

**파일**: `ShadowMapPass.cpp`

6개의 cube face를 위한 view-projection matrix 계산:

```cpp
void FShadowMapPass::CalculatePointLightViewProj(UPointLightComponent* Light, FMatrix OutViewProj[6])
{
    FVector LightPos = Light->GetWorldLocation();
    float Near = 1.0f;
    float Far = Light->GetAttenuationRadius();

    // 90 degree FOV for cube faces (π/2 radians)
    FMatrix Proj = ShadowMatrixHelper::CreatePerspectiveFovLH(
        PI / 2.0f,  // Fixed 90-degree FOV
        1.0f,       // Aspect ratio 1:1 (square faces)
        Near, Far
    );

    // 6 directions for cube faces
    FVector Directions[6] = {
        FVector(1.0f, 0.0f, 0.0f),   // +X (Right)
        FVector(-1.0f, 0.0f, 0.0f),  // -X (Left)
        FVector(0.0f, 1.0f, 0.0f),   // +Y (Top)
        FVector(0.0f, -1.0f, 0.0f),  // -Y (Bottom)
        FVector(0.0f, 0.0f, 1.0f),   // +Z (Front)
        FVector(0.0f, 0.0f, -1.0f)   // -Z (Back)
    };

    // Up vectors (avoid gimbal lock for Y-axis faces)
    FVector Ups[6] = {
        FVector(0.0f, 1.0f, 0.0f),   // +X: Y-Up
        FVector(0.0f, 1.0f, 0.0f),   // -X: Y-Up
        FVector(0.0f, 0.0f, -1.0f),  // +Y: -Z-Up (gimbal lock 방지)
        FVector(0.0f, 0.0f, 1.0f),   // -Y: +Z-Up (gimbal lock 방지)
        FVector(0.0f, 1.0f, 0.0f),   // +Z: Y-Up
        FVector(0.0f, 1.0f, 0.0f)    // -Z: Y-Up
    };

    for (int i = 0; i < 6; i++)
    {
        FVector Target = LightPos + Directions[i];
        FMatrix View = ShadowMatrixHelper::CreateLookAtLH(LightPos, Target, Ups[i]);
        OutViewProj[i] = View * Proj;
    }
}
```

**핵심 포인트**:
- FOV는 **90도 고정** (cube face는 정확히 90도여야 함)
- Y축 방향 face는 **다른 Up vector** 사용 (gimbal lock 회피)
- 모든 face의 aspect ratio는 **1:1**

---

### Phase 2: RenderPointShadowMap (6개 면 렌더링)

**파일**: `ShadowMapPass.cpp`

Scene을 6번 렌더링하여 각 cube face에 depth 저장:

```cpp
void FShadowMapPass::RenderPointShadowMap(UPointLightComponent* Light,
    const TArray<UStaticMeshComponent*>& Meshes)
{
    FCubeShadowMapResource* ShadowMap = GetOrCreateCubeShadowMap(Light);
    if (!ShadowMap || !ShadowMap->IsValid())
        return;

    // 1. Pipeline 설정 (Linear distance shader 사용)
    FPipelineInfo ShadowPipelineInfo = {
        PointLightShadowInputLayout,
        PointLightShadowVS,
        RastState,
        ShadowDepthStencilState,
        PointLightShadowPS,  // ← Pixel shader로 linear distance 출력
        nullptr,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };
    Pipeline->UpdatePipeline(ShadowPipelineInfo);

    // 2. Light position과 range를 constant buffer로 전달
    FPointLightShadowParams Params;
    Params.LightPosition = Light->GetWorldLocation();
    Params.LightRange = Light->GetAttenuationRadius();
    FRenderResourceFactory::UpdateConstantBufferData(PointLightShadowParamsBuffer, Params);
    Pipeline->SetConstantBuffer(2, EShaderType::PS, PointLightShadowParamsBuffer);

    // 3. 6개 View-Projection 계산
    FMatrix ViewProj[6];
    CalculatePointLightViewProj(Light, ViewProj);

    // 4. 6개 면 렌더링
    for (int Face = 0; Face < 6; Face++)
    {
        // 4.1. 이 face의 DSV 설정
        Pipeline->SetRenderTargets(1, &NullRTV, ShadowMap->ShadowDSVs[Face].Get());
        DeviceContext->RSSetViewports(1, &ShadowMap->ShadowViewport);
        DeviceContext->ClearDepthStencilView(ShadowMap->ShadowDSVs[Face].Get(),
            D3D11_CLEAR_DEPTH, 1.0f, 0);

        // 4.2. 이 face의 ViewProj를 constant buffer로 전달
        FShadowViewProjConstant CBData;
        CBData.ViewProjection = ViewProj[Face];
        FRenderResourceFactory::UpdateConstantBufferData(ShadowViewProjConstantBuffer, CBData);
        Pipeline->SetConstantBuffer(1, EShaderType::VS, ShadowViewProjConstantBuffer);

        // 4.3. 모든 mesh 렌더링
        for (auto Mesh : Meshes)
        {
            if (Mesh->IsVisible())
            {
                // Model transform 업데이트
                FMatrix WorldMatrix = Mesh->GetWorldTransformMatrix();
                FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
                Pipeline->SetConstantBuffer(0, EShaderType::VS, ConstantBufferModel);

                // Draw
                Pipeline->SetVertexBuffer(VertexBuffer, sizeof(FNormalVertex));
                Pipeline->SetIndexBuffer(IndexBuffer, 0);
                Pipeline->DrawIndexed(IndexCount, 0, 0);
            }
        }
    }
}
```

**핵심 포인트**:
- **6번 렌더링**: 각 cube face마다 scene 전체를 렌더링
- **DSV 전환**: 각 face마다 다른 DSV 사용 (`ShadowDSVs[0]` ~ `ShadowDSVs[5]`)
- **ViewProj 업데이트**: 각 face마다 다른 view-projection matrix

---

### Phase 3: GetOrCreateCubeShadowMap (Cube Shadow Map 관리)

**파일**: `ShadowMapPass.cpp`

Cube shadow map 리소스 생성 및 캐싱:

```cpp
FCubeShadowMapResource* FShadowMapPass::GetOrCreateCubeShadowMap(UPointLightComponent* Light)
{
    FCubeShadowMapResource* ShadowMap = nullptr;

    auto It = PointShadowMaps.find(Light);
    if (It == PointShadowMaps.end())
    {
        // 새로 생성
        ShadowMap = new FCubeShadowMapResource();
        ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
        PointShadowMaps[Light] = ShadowMap;
    }
    else
    {
        ShadowMap = It->second;

        // 해상도 변경 시 재생성
        if (ShadowMap->NeedsResize(Light->GetShadowMapResolution()))
        {
            ShadowMap->Initialize(Device, Light->GetShadowMapResolution());
        }
    }

    return ShadowMap;
}

FCubeShadowMapResource* FShadowMapPass::GetPointShadowMap(UPointLightComponent* Light)
{
    auto It = PointShadowMaps.find(Light);
    return It != PointShadowMaps.end() ? It->second : nullptr;
}
```

**FCubeShadowMapResource 구조** (`ShadowMapResources.h`):

```cpp
struct FCubeShadowMapResource
{
    ComPtr<ID3D11Texture2D> ShadowTextureCube = nullptr;      // Cube texture
    ComPtr<ID3D11DepthStencilView> ShadowDSVs[6] = { nullptr }; // 6 DSVs (각 면)
    ComPtr<ID3D11ShaderResourceView> ShadowSRV = nullptr;      // 1 SRV (cube)
    D3D11_VIEWPORT ShadowViewport = {};
    uint32 Resolution = 512;

    void Initialize(ID3D11Device* Device, uint32 InResolution);
    void Release();
    bool NeedsResize(uint32 NewResolution) const;
    bool IsValid() const;
};
```

**Initialize 구현** (`ShadowMapResources.cpp`):

```cpp
void FCubeShadowMapResource::Initialize(ID3D11Device* Device, uint32 InResolution)
{
    Release();  // 기존 리소스 해제
    Resolution = InResolution;

    // 1. Cube Texture 생성 (ArraySize = 6)
    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width = Resolution;
    TexDesc.Height = Resolution;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 6;  // Cube map은 6개 array slice
    TexDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.Usage = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    TexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;  // ← Cube flag

    Device->CreateTexture2D(&TexDesc, nullptr, &ShadowTextureCube);

    // 2. 6개 DSV 생성 (각 array slice마다)
    D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
    DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
    DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    DSVDesc.Texture2DArray.MipSlice = 0;
    DSVDesc.Texture2DArray.ArraySize = 1;  // 한 번에 1개 slice

    for (int i = 0; i < 6; i++)
    {
        DSVDesc.Texture2DArray.FirstArraySlice = i;
        Device->CreateDepthStencilView(ShadowTextureCube.Get(), &DSVDesc, &ShadowDSVs[i]);
    }

    // 3. Cube SRV 생성
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;  // ← Cube dimension
    SRVDesc.TextureCube.MipLevels = 1;

    Device->CreateShaderResourceView(ShadowTextureCube.Get(), &SRVDesc, &ShadowSRV);

    // 4. Viewport 설정
    ShadowViewport.TopLeftX = 0.0f;
    ShadowViewport.TopLeftY = 0.0f;
    ShadowViewport.Width = static_cast<float>(Resolution);
    ShadowViewport.Height = static_cast<float>(Resolution);
    ShadowViewport.MinDepth = 0.0f;
    ShadowViewport.MaxDepth = 1.0f;
}
```

**핵심 포인트**:
- `D3D11_RESOURCE_MISC_TEXTURECUBE` flag 필수
- 6개 DSV (각 face용) + 1개 SRV (cube 전체)
- `D3D11_DSV_DIMENSION_TEXTURE2DARRAY`로 각 face 선택

---

### Phase 4: Rasterizer State 캐싱

**파일**: `ShadowMapPass.cpp`

매 프레임 GPU 리소스 생성을 방지하기 위한 캐싱:

```cpp
ID3D11RasterizerState* FShadowMapPass::GetOrCreateRasterizerState(UPointLightComponent* Light)
{
    // 이미 생성된 state가 있으면 재사용
    auto It = PointRasterizerStates.find(Light);
    if (It != PointRasterizerStates.end())
        return It->second;

    // 새로 생성
    const auto& Renderer = URenderer::GetInstance();
    D3D11_RASTERIZER_DESC RastDesc = {};
    ShadowRasterizerState->GetDesc(&RastDesc);

    // Light별 DepthBias 설정
    RastDesc.DepthBias = static_cast<INT>(Light->GetShadowBias() * 100000.0f);
    RastDesc.SlopeScaledDepthBias = Light->GetShadowSlopeBias();

    ID3D11RasterizerState* NewState = nullptr;
    Renderer.GetDevice()->CreateRasterizerState(&RastDesc, &NewState);

    // 캐시에 저장
    PointRasterizerStates[Light] = NewState;

    return NewState;
}
```

---

### Phase 5: PointLightComponent 파라미터 (이미 완료)

**파일**: `PointLightComponent.cpp`

Shadow 파라미터는 이미 FPointLightInfo에 포함되어 있음:

```cpp
FPointLightInfo UPointLightComponent::GetPointlightInfo() const
{
    FPointLightInfo Info;
    Info.Color = FVector4(LightColor, 1.0f);
    Info.Position = GetWorldLocation();
    Info.Intensity = Intensity;
    Info.Range = AttenuationRadius;
    Info.DistanceFalloffExponent = DistanceFalloffExponent;

    // Shadow parameters (already exists)
    Info.CastShadow = GetCastShadows() ? 1u : 0u;
    Info.ShadowBias = GetShadowBias();
    Info.ShadowSlopeBias = GetShadowSlopeBias();
    Info.ShadowSharpen = GetShadowSharpen();
    Info.Padding = FVector2(0.0f, 0.0f);

    return Info;
}
```

---

### Phase 6: PointLightShadowDepth.hlsl (Linear Distance Shader)

**파일**: `PointLightShadowDepth.hlsl` (새로 생성)

Point Light shadow map에 **linear distance**를 저장하는 전용 shader:

```hlsl
// Point Light Shadow Depth Shader
// Linear distance를 depth로 저장

cbuffer Model : register(b0)
{
    row_major float4x4 World;
}

cbuffer ViewProj : register(b1)
{
    row_major float4x4 ViewProjection;
};

cbuffer PointLightShadowParams : register(b2)
{
    float3 LightPosition;
    float LightRange;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float4 Tangent : TANGENT;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
};

struct PS_OUTPUT
{
    float Depth : SV_Depth;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    // World space position
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
    Output.WorldPosition = WorldPos.xyz;

    // Clip space position
    Output.Position = mul(WorldPos, ViewProjection);

    return Output;
}

PS_OUTPUT mainPS(PS_INPUT Input)
{
    PS_OUTPUT Output;

    // Calculate linear distance from light to pixel
    float Distance = length(Input.WorldPosition - LightPosition);

    // Normalize to [0, 1] range
    Output.Depth = saturate(Distance / LightRange);

    return Output;
}
```

**핵심 포인트**:
- **Pixel Shader에서 depth 출력** (`SV_Depth` semantic)
- **Linear distance 계산**: `length(WorldPos - LightPos)`
- **Normalize**: `distance / range`로 [0,1] 범위로 변환

---

### Phase 7: UberLit.hlsl - TextureCube 샘플링

**파일**: `UberLit.hlsl`

Cube shadow map 샘플링 및 shadow factor 계산:

```hlsl
// Shadow Maps
Texture2D DirectionalShadowMap : register(t10);
Texture2D SpotShadowMap : register(t11);
TextureCube PointShadowMap : register(t12);  // ← Cube map

SamplerState SamplerWrap : register(s0);
SamplerComparisonState ShadowSampler : register(s1);
```

**CalculatePointShadowFactor() 함수**:

```hlsl
float CalculatePointShadowFactor(FPointLightInfo Light, float3 WorldPos)
{
    // If shadow is disabled, return fully lit (1.0)
    if (Light.CastShadow == 0)
        return 1.0f;

    // Calculate direction from light to pixel (for cube map sampling)
    float3 LightToPixel = WorldPos - Light.Position;
    float Distance = length(LightToPixel);

    // If too close to light or outside range, assume lit
    if (Distance < 1e-6 || Distance > Light.Range)
        return 1.0f;

    // Normalize direction for cube sampling
    float3 SampleDir = LightToPixel / Distance;

    // Sample the cube shadow map to get stored linear distance
    // The shadow map stores: saturate(distance / range)
    // Use SampleLevel instead of Sample for VS compatibility (Gouraud shading)
    float StoredDistance = PointShadowMap.SampleLevel(SamplerWrap, SampleDir, 0).r;

    // Convert current distance to normalized [0,1] range
    float CurrentDistance = Distance / Light.Range;

    // Add small bias to prevent shadow acne
    float Bias = 0.005f;

    // Manual depth comparison (hard shadow)
    // If current distance > stored distance, pixel is in shadow
    float ShadowFactor = (CurrentDistance - Bias) > StoredDistance ? 0.0f : 1.0f;

    return ShadowFactor;
}
```

**핵심 포인트**:
- **3D direction 샘플링**: `PointShadowMap.SampleLevel(sampler, direction, 0)`
- **SampleLevel 사용**: Vertex Shader (Gouraud) 호환성
- **Manual comparison**: `CurrentDistance > StoredDistance ? 0 : 1`
- **SampleCmp 사용 불가**: TextureCube는 comparison sampler 미지원

---

### Phase 8: CalculatePointLight()에 Shadow 적용

**파일**: `UberLit.hlsl`

Point Light 계산에 shadow factor 적용:

```hlsl
FIllumination CalculatePointLight(FPointLightInfo Info, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    FIllumination Result = (FIllumination) 0;

    float3 LightDir = Info.Position - WorldPos;
    float Distance = length(LightDir);

    if (Distance < 1e-6 || Info.Range < 1e-6 || Distance > Info.Range)
        return Result;

    LightDir = SafeNormalize3(LightDir);
    float NdotL = saturate(dot(WorldNormal, LightDir));
    float Attenuation = pow(saturate(1.0f - Distance / Info.Range), Info.DistanceFalloffExponent);

    // Calculate shadow factor (0 = shadow, 1 = lit)
    float ShadowFactor = CalculatePointShadowFactor(Info, WorldPos);

    // Diffuse illumination (affected by shadow)
    Result.Diffuse = Info.Color * Info.Intensity * NdotL * Attenuation * ShadowFactor;

#if LIGHTING_MODEL_BLINNPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong) - also affected by shadow
    float3 WorldToCameraVector = SafeNormalize3(ViewPos - WorldPos);
    float3 WorldToLightVector = LightDir;

    float3 H = SafeNormalize3(WorldToLightVector + WorldToCameraVector);
    float CosTheta = saturate(dot(WorldNormal, H));
    float Spec = CosTheta < 1e-6 ? 0.0f : ((Ns + 8.0f) / (8.0f * PI)) * pow(CosTheta, Ns);
    Result.Specular = Info.Color * Info.Intensity * Spec * Attenuation * ShadowFactor;
#endif

    return Result;
}
```

---

### Phase 9: StaticMeshPass에서 Cube Shadow Map Binding

**파일**: `StaticMeshPass.cpp`

Shader에 cube shadow map SRV 바인딩:

```cpp
// Bind point light shadow map if available (cube map)
if (!Context.PointLights.empty() && Context.PointLights[0]->GetCastShadows())
{
    UPointLightComponent* PointLight = Context.PointLights[0];
    FShadowMapPass* ShadowPass = Renderer.GetShadowMapPass();
    if (ShadowPass)
    {
        FCubeShadowMapResource* CubeShadowMap = ShadowPass->GetPointShadowMap(PointLight);
        if (CubeShadowMap && CubeShadowMap->IsValid())
        {
            Pipeline->SetShaderResourceView(12, EShaderType::PS, CubeShadowMap->ShadowSRV.Get());
        }
    }
}
```

**Resource Hazard 방지** (Pass 종료 시 unbind):

```cpp
// Unbind shadow maps to prevent resource hazards
Pipeline->SetShaderResourceView(10, EShaderType::PS, nullptr);  // Directional
Pipeline->SetShaderResourceView(11, EShaderType::PS, nullptr);  // Spot
Pipeline->SetShaderResourceView(12, EShaderType::PS, nullptr);  // Point (cube)
```

---

## Linear Distance 방식

### 왜 Linear Distance가 필요한가?

**문제**: Perspective Depth vs Linear Distance

Cube shadow map의 각 face는 **서로 다른 view space**를 가집니다:

```
Face +X: View space Z = world space X 거리
Face +Y: View space Z = world space Y 거리
Face +Z: View space Z = world space Z 거리
```

만약 **perspective projection depth**를 그대로 사용하면:

```hlsl
// ❌ 잘못된 방식 (Perspective depth)
float CurrentDepth = (Far / (Far - Near)) * (1.0f - Near / ViewZ);
```

- 각 face마다 **ViewZ의 의미가 다름**
- Light에서 같은 거리에 있어도 **face마다 다른 depth 값**
- 결과: **6개의 spotlight처럼 보이는 artifact**

### Linear Distance 솔루션

**World space euclidean distance**를 사용:

```hlsl
// ✅ 올바른 방식 (Linear distance)
float Distance = length(WorldPos - LightPos);
float NormalizedDistance = saturate(Distance / LightRange);
```

- 모든 방향에서 **일관된 거리 측정**
- Face 경계에서도 **부드러운 전환**
- **Omnidirectional shadow** 달성

### 구현 방법

**Shadow Map 렌더링 시** (PointLightShadowDepth.hlsl):

```hlsl
PS_OUTPUT mainPS(PS_INPUT Input)
{
    PS_OUTPUT Output;

    // Linear distance 계산
    float Distance = length(Input.WorldPosition - LightPosition);

    // [0,1] 범위로 normalize
    Output.Depth = saturate(Distance / LightRange);

    return Output;
}
```

**Shadow 비교 시** (UberLit.hlsl):

```hlsl
// Shadow map에서 저장된 거리 읽기
float StoredDistance = PointShadowMap.SampleLevel(SamplerWrap, SampleDir, 0).r;

// 현재 픽셀의 거리 계산
float CurrentDistance = Distance / Light.Range;

// 비교
float ShadowFactor = (CurrentDistance - Bias) > StoredDistance ? 0.0f : 1.0f;
```

---

## Soft Shadows (PCF)

### PCF (Percentage Closer Filtering) 개요

**Hard Shadow의 문제점**:
- 그림자 경계가 **날카롭고 계단 현상** 발생
- 비현실적인 외관

**Soft Shadow 솔루션**:
- 여러 개의 샘플을 평균내어 **부드러운 경계** 생성
- 현실적인 penumbra (반그림자) 효과

### 3D Offset Pattern

Cube map은 **3D direction**으로 샘플링하므로, **3D offset pattern**이 필요합니다:

```hlsl
// Point Light PCF Sample Offsets
// 20 samples in 3D space
static const float3 CubePCFOffsets[20] = {
    // 6-direction axis-aligned offsets
    float3( 1.0,  0.0,  0.0), float3(-1.0,  0.0,  0.0),
    float3( 0.0,  1.0,  0.0), float3( 0.0, -1.0,  0.0),
    float3( 0.0,  0.0,  1.0), float3( 0.0,  0.0, -1.0),

    // 12-direction face-diagonal offsets
    float3( 1.0,  1.0,  0.0), float3( 1.0, -1.0,  0.0),
    float3(-1.0,  1.0,  0.0), float3(-1.0, -1.0,  0.0),
    float3( 1.0,  0.0,  1.0), float3( 1.0,  0.0, -1.0),
    float3(-1.0,  0.0,  1.0), float3(-1.0,  0.0, -1.0),
    float3( 0.0,  1.0,  1.0), float3( 0.0,  1.0, -1.0),
    float3( 0.0, -1.0,  1.0), float3( 0.0, -1.0, -1.0),

    // 2-direction 3D diagonal offsets
    float3( 1.0,  1.0,  1.0), float3(-1.0, -1.0, -1.0)
};
```

**패턴 설명**:
- **6개 축 방향**: 큐브의 6면 방향
- **12개 면 대각선**: 큐브 모서리 방향
- **2개 공간 대각선**: 큐브 코너 방향

### PCF 구현

```hlsl
float CalculatePointShadowFactor(FPointLightInfo Light, float3 WorldPos)
{
    if (Light.CastShadow == 0)
        return 1.0f;

    float3 LightToPixel = WorldPos - Light.Position;
    float Distance = length(LightToPixel);

    if (Distance < 1e-6 || Distance > Light.Range)
        return 1.0f;

    float3 SampleDir = LightToPixel / Distance;
    float CurrentDistance = Distance / Light.Range;
    float Bias = 0.005f;

    // PCF 파라미터
    float FilterRadius = 0.01f;  // 그림자 부드러움 조절

    float ShadowFactor = 0.0f;
    float TotalSamples = 1.0f;

    // Center sample
    float StoredDistance = PointShadowMap.SampleLevel(SamplerWrap, SampleDir, 0).r;
    ShadowFactor += (CurrentDistance - Bias) > StoredDistance ? 0.0f : 1.0f;

    // Offset samples (20개)
    [unroll]
    for (int i = 0; i < 20; i++)
    {
        // 3D offset 적용
        float3 OffsetDir = normalize(SampleDir + CubePCFOffsets[i] * FilterRadius);

        // Shadow map 샘플링
        StoredDistance = PointShadowMap.SampleLevel(SamplerWrap, OffsetDir, 0).r;

        // 비교 및 누적
        ShadowFactor += (CurrentDistance - Bias) > StoredDistance ? 0.0f : 1.0f;
        TotalSamples += 1.0f;
    }

    // 평균 (21개 샘플)
    return ShadowFactor / TotalSamples;
}
```

### 품질 vs 성능 튜닝

**FilterRadius** (그림자 부드러움):
```hlsl
float FilterRadius = 0.005f;  // 살짝 부드러움
float FilterRadius = 0.01f;   // 중간 (기본값)
float FilterRadius = 0.02f;   // 꽤 부드러움
float FilterRadius = 0.05f;   // 매우 부드러움
```

**Sample Count** (품질 vs 성능):
```hlsl
for (int i = 0; i < 8; i++)   // 9 samples  - 빠름, 품질 낮음
for (int i = 0; i < 12; i++)  // 13 samples - 균형
for (int i = 0; i < 20; i++)  // 21 samples - 기본값, 권장
```

**성능 비교**:
- **Hard Shadow**: 1 sample per pixel
- **PCF (21 samples)**: 21 samples per pixel (~21배 비용)
- **실제 성능 영향**: 약 10-15% FPS 감소 (clustered culling 덕분에 완화)

---

## 트러블슈팅

### 1. 그림자가 6개의 Spotlight처럼 보임

**증상**:
- 바닥에 십자가 모양 그림자
- 각 cube face 방향으로 포물선 형태 밝은 영역
- Face 경계가 명확히 보임

**원인**:
- Perspective projection depth 사용
- View space Z가 face마다 다름
- Linear distance로 변환하지 않음

**해결책**:
```hlsl
// ❌ 잘못된 코드
float CurrentDepth = (Far / (Far - Near)) * (1.0f - Near / ViewZ);

// ✅ 올바른 코드
float Distance = length(WorldPos - LightPos);
float CurrentDistance = Distance / Light.Range;
```

**확인 사항**:
- `PointLightShadowDepth.hlsl` 사용 확인
- Pixel shader에서 linear distance 출력 확인
- Shadow map 렌더링 시 `PointLightShadowPS` 바인딩 확인

---

### 2. 모든 영역이 검은색 (그림자)

**증상**:
- Point Light 위에 있어도 그림자
- 조명이 전혀 비추지 않음

**원인**:
- Shader 컴파일 에러
- TextureCube 샘플링 문제
- Bias가 너무 큼

**해결책**:

1. **Shader 컴파일 에러 확인**:
   ```
   error X4532: cannot map expression to vs_5_0 instruction set
   ```
   - `Sample()` → `SampleLevel()` 변경 필요 (VS 호환성)

2. **Bias 조정**:
   ```hlsl
   float Bias = 0.005f;  // 기본값
   // 너무 크면 모든 영역이 그림자로 보임
   ```

3. **Loop Unroll 문제**:
   ```
   error X3511: unable to unroll loop
   ```
   - `[unroll]` → `[loop]` 변경

---

### 3. Shadow Acne (점박이 그림자)

**증상**:
- 그림자 내부에 밝은 점들
- 표면에 얼룩덜룩한 패턴

**원인**:
- Floating point precision
- Self-shadowing

**해결책**:
```hlsl
// Bias 증가
float Bias = 0.01f;  // 0.005f에서 증가

// 또는 PointLightComponent의 ShadowBias 조정
Light->SetShadowBias(0.01f);
```

---

### 4. Peter Panning (그림자가 물체에서 떨어짐)

**증상**:
- 그림자가 물체와 분리됨
- 공중에 떠 있는 듯한 그림자

**원인**:
- Bias가 너무 큼

**해결책**:
```hlsl
// Bias 감소
float Bias = 0.003f;  // 0.005f에서 감소
```

---

### 5. PCF 성능 문제

**증상**:
- FPS 급격히 감소
- Point Light shadow 활성화 시 버벅임

**해결책**:

1. **Sample count 감소**:
   ```hlsl
   for (int i = 0; i < 8; i++)  // 20에서 8로 감소
   ```

2. **거리 기반 LOD**:
   ```hlsl
   // 먼 거리면 샘플 수 감소
   int SampleCount = Distance > 50.0f ? 4 : 20;
   ```

3. **FilterRadius 감소**:
   ```hlsl
   float FilterRadius = 0.005f;  // 0.01f에서 감소
   ```

---

### 6. Gimbal Lock (Y축 방향 이상)

**증상**:
- +Y 또는 -Y 방향 face에서 이상한 그림자
- 회전이 뒤집힘

**원인**:
- Y축 방향 face의 Up vector 설정 문제

**해결책**:
```cpp
// CalculatePointLightViewProj()에서 확인
FVector Ups[6] = {
    FVector(0.0f, 1.0f, 0.0f),   // +X: Y-Up
    FVector(0.0f, 1.0f, 0.0f),   // -X: Y-Up
    FVector(0.0f, 0.0f, -1.0f),  // +Y: -Z-Up (중요!)
    FVector(0.0f, 0.0f, 1.0f),   // -Y: +Z-Up (중요!)
    FVector(0.0f, 1.0f, 0.0f),   // +Z: Y-Up
    FVector(0.0f, 1.0f, 0.0f)    // -Z: Y-Up
};
```

---

## 성능 최적화

### 현재 구현의 성능 특성

**렌더링 비용**:
- **Directional Light**: 1× scene render
- **Spot Light**: 1× scene render
- **Point Light**: **6× scene render** (cube faces)

**메모리 비용**:
- **512×512 × 6 faces × 4 bytes = 6 MB** per light

**Shader 비용**:
- **Hard Shadow**: 1 sample per pixel
- **PCF (21 samples)**: 21 samples per pixel

### 최적화 전략

#### 1. View Frustum Culling (권장)

화면 밖 Point Light는 shadow map 렌더링 스킵:

```cpp
// ShadowMapPass.cpp
bool IsLightVisibleInView(UPointLightComponent* Light, UCamera* Camera)
{
    FVector LightPos = Light->GetWorldLocation();
    float Radius = Light->GetAttenuationRadius();

    // Sphere-frustum intersection test
    return Camera->SphereInFrustum(LightPos, Radius);
}

void FShadowMapPass::Execute(FRenderingContext& Context)
{
    for (auto Light : Context.PointLights)
    {
        if (!IsLightVisibleInView(Light, Context.Camera))
            continue;  // 화면 밖이면 스킵

        RenderPointShadowMap(Light, meshes);
    }
}
```

**예상 성능 향상**: 화면 밖 라이트 수에 비례 (50-90% 절감 가능)

---

#### 2. Distance-based LOD

거리에 따라 shadow map 해상도와 PCF 품질 조절:

```cpp
// 거리 기반 해상도
uint32 Resolution = 512;
float Distance = (Light->GetWorldLocation() - Camera->GetLocation()).Length();

if (Distance < 20.0f)
    Resolution = 1024;  // 가까우면 고해상도
else if (Distance > 100.0f)
    Resolution = 256;   // 멀면 저해상도
```

```hlsl
// 거리 기반 PCF 샘플 수
int SampleCount = 20;
if (Distance > 50.0f)
    SampleCount = 8;   // 멀면 적은 샘플
```

**예상 성능 향상**: 20-40% 절감

---

#### 3. Shadow Max Distance

너무 먼 라이트는 shadow 비활성화:

```cpp
const float ShadowMaxDistance = 100.0f;

for (auto Light : Context.PointLights)
{
    float Distance = (Light->GetWorldLocation() - Camera->GetLocation()).Length();
    if (Distance > ShadowMaxDistance)
        continue;  // 너무 멀면 스킵

    RenderPointShadowMap(Light, meshes);
}
```

**예상 성능 향상**: Scene 크기에 따라 다름

---

#### 4. Dynamic Resolution

프레임레이트에 따라 동적으로 해상도 조절:

```cpp
// 60 FPS 이상: 512×512
// 30-60 FPS: 256×256
// 30 FPS 이하: 128×128

uint32 Resolution = 512;
if (FPS < 30.0f)
    Resolution = 128;
else if (FPS < 60.0f)
    Resolution = 256;
```

---

#### 5. Sample Count 최적화

**권장 설정**:
- **High Quality**: 21 samples (현재)
- **Medium Quality**: 13 samples (균형)
- **Low Quality**: 9 samples (성능 우선)

```hlsl
#define POINT_SHADOW_QUALITY 1  // 0=Low, 1=Medium, 2=High

#if POINT_SHADOW_QUALITY == 2
    for (int i = 0; i < 20; i++)  // 21 samples
#elif POINT_SHADOW_QUALITY == 1
    for (int i = 0; i < 12; i++)  // 13 samples
#else
    for (int i = 0; i < 8; i++)   // 9 samples
#endif
```

---

### 성능 측정 가이드

**측정 항목**:
1. **Shadow Map Pass 시간**: 6-face rendering 비용
2. **Static Mesh Pass 시간**: PCF 샘플링 비용
3. **전체 Frame Time**: 종합 성능
4. **GPU 메모리**: Cube shadow map 메모리

**벤치마크 시나리오**:
- 1개 Point Light: Baseline
- 10개 Point Light: 다중 라이트
- 100개 Point Light: Extreme case (clustered culling 효과 확인)

---

## 테스트 방법

### 기본 테스트

1. **Point Light 생성**:
   - Scene에 Point Light Actor 배치
   - `CastShadows = true` 설정
   - `AttenuationRadius` 조정

2. **Shadow Caster 배치**:
   - Point Light 주변에 Static Mesh 배치
   - 큐브, 구, 평면 등 다양한 형태 테스트

3. **시각적 확인**:
   - 그림자가 모든 방향으로 드리워지는지 확인
   - Face 경계에서 부드러운 전환 확인
   - PCF로 그림자 경계가 부드러운지 확인

### 고급 테스트

1. **Multi-Light 테스트**:
   - 여러 Point Light 배치
   - 첫 번째 라이트만 shadow casting 확인 (현재 제한)

2. **성능 테스트**:
   - FPS 측정 (shadow on/off 비교)
   - 다양한 해상도 테스트 (256, 512, 1024)
   - PCF sample count 변경 (9, 13, 21)

3. **Edge Case 테스트**:
   - Light 범위 밖 물체
   - Light 내부 물체
   - 겹치는 그림자
   - 얇은 물체 (shadow acne 확인)

### 디버깅 도구

1. **Shadow Map Visualization**:
   - Cube map의 각 face를 화면에 출력
   - Linear distance 값 확인

2. **Face Direction Debug**:
   - 각 face가 올바른 방향을 보는지 확인
   - Gimbal lock 발생 여부 확인

3. **Distance Debug**:
   - StoredDistance vs CurrentDistance 출력
   - Bias 효과 확인

---

## 참고 자료

### 관련 문서

- [DirectionalLight.md](DirectionalLight.md) - Directional Light shadow mapping
- [SpotLight.md](SpotLight.md) - Spot Light shadow mapping
- [README.md](../README.md) - FutureEngine 전체 문서

### 핵심 파일

**C++ 구현**:
- `Engine/Source/Render/RenderPass/Public/ShadowMapPass.h`
- `Engine/Source/Render/RenderPass/Private/ShadowMapPass.cpp`
- `Engine/Source/Render/RenderPass/Private/StaticMeshPass.cpp`
- `Engine/Source/Texture/Public/ShadowMapResources.h`
- `Engine/Source/Texture/Private/ShadowMapResources.cpp`
- `Engine/Source/Component/Public/PointLightComponent.h`
- `Engine/Source/Component/Private/PointLightComponent.cpp`

**HLSL 구현**:
- `Engine/Asset/Shader/PointLightShadowDepth.hlsl` - Linear distance shader
- `Engine/Asset/Shader/UberLit.hlsl` - Main lighting shader
- `Engine/Asset/Shader/LightStructures.hlsli` - Light info structures

### 외부 참고 자료

- [LearnOpenGL - Point Shadows](https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows)
- [Microsoft DirectX SDK - Cube Maps](https://docs.microsoft.com/en-us/windows/win32/direct3d9/cube-textures)
- [GPU Gems - Chapter 12: Omnidirectional Shadow Mapping](https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-12-omnidirectional-shadow-mapping)

---

## 요약

### 구현 체크리스트

- ✅ **FCubeShadowMapResource**: 6 DSVs + 1 Cube SRV
- ✅ **CalculatePointLightViewProj**: 6개 view-projection matrix (90° FOV)
- ✅ **RenderPointShadowMap**: 6-face rendering loop
- ✅ **PointLightShadowDepth.hlsl**: Linear distance output shader
- ✅ **CalculatePointShadowFactor**: TextureCube sampling + manual comparison
- ✅ **PCF Implementation**: 21 samples (3D offsets)
- ✅ **StaticMeshPass binding**: t12 slot cube shadow map
- ✅ **Resource hazard prevention**: t12 unbind

### 주요 차이점

| DirectionalLight | SpotLight | **PointLight** |
|-----------------|-----------|----------------|
| Texture2D | Texture2D | **TextureCube** |
| 1× render | 1× render | **6× render** |
| Orthographic | Perspective | **Perspective × 6** |
| UV sampling | UV sampling | **Direction sampling** |
| SampleCmp | SampleCmp | **Manual comparison** |
| Perspective depth | Perspective depth | **Linear distance** |

### 성능 고려사항

- **6× rendering cost**: 가장 비싼 shadow type
- **View frustum culling**: 필수 최적화
- **Distance-based LOD**: 권장 최적화
- **PCF sample count**: 품질 vs 성능 균형
- **Clustered culling**: 다중 라이트 성능 개선

---

**마지막 업데이트**: 2025-10-25
**문서 버전**: 1.0
**다음 개선 사항**: Multi-light shadow support, Cascade cube shadows
