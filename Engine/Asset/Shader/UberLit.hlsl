// UberLit.hlsl - Uber Shader with Multiple Lighting Models
// Supports: Gouraud, Lambert, Phong lighting models

// =============================================================================
// <주의사항>
// normalize 대신 SafeNormalize 함수를 사용하세요.
// normalize에는 영벡터 입력시 NaN이 발생할 수 있습니다. (div by zero 가드가 없음)
// =============================================================================
#include "LightStructures.hlsli"

#define NUM_POINT_LIGHT 8
#define NUM_SPOT_LIGHT 8
#define ADD_ILLUM(a, b) { (a).Ambient += (b).Ambient; (a).Diffuse += (b).Diffuse; (a).Specular += (b).Specular; }

static const float PI = 3.14159265358979323846f;

#define ATLASSIZE 8192.0f
#define ATLASTILECOUNT 8.0f
#define ATLASGRIDSIZE 1.0f / ATLASTILECOUNT

#define GET_TILE_SIZE(resolution) (1.0f / (ATLASSIZE / resolution))

#define MAX_POINT_LIGHT_NUM 8
#define MAX_SPOTLIGHT_NUM 8

// reflectance와 곱해지기 전
// 표면에 도달한 빛의 조명 기여량
struct FIllumination
{
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
};

// Constant Buffers
cbuffer Model : register(b0)
{
    row_major float4x4 World;
}

cbuffer Camera : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 ViewWorldLocation;
    float NearClip;
    float FarClip;
}

cbuffer MaterialConstants : register(b2)
{
    float4 Ka; // Ambient color
    float4 Kd; // Diffuse color
    float4 Ks; // Specular color
    float Ns;  // Specular exponent
    float Ni;  // Index of refraction
    float D;   // Dissolve factor
    uint MaterialFlags; // Which textures are available (bitfield)
    float Time;
}

cbuffer GlobalLightConstant : register(b3)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional;
};
cbuffer ClusterSliceInfo : register(b4)
{
    uint ClusterSliceNumX;
    uint ClusterSliceNumY;
    uint ClusterSliceNumZ;
    uint LightMaxCountPerCluster;
    uint SpotLightIntersectOption;
    uint Orthographic;
    uint2 padding;
};

cbuffer LightCountInfo : register(b5)
{
    uint PointLightCount;
    uint SpotLightCount;
    float2 Padding;
};

cbuffer CascadeShadowMapData : register(b6)
{
    row_major float4x4 CascadeView;      // 64 bytes
    row_major float4x4 CascadeProj[8];   // 512 bytes
    float4 SplitDistance[8];    // 128 bytes (HLSL: each float is 16-byte aligned)
    uint SplitNum;             // 16 bytes (with padding)
    // Total: 720 bytes
}

StructuredBuffer<int> PointLightIndices : register(t6);
StructuredBuffer<int> SpotLightIndices : register(t7);
StructuredBuffer<FPointLightInfo> PointLightInfos : register(t8);
StructuredBuffer<FSpotLightInfo> SpotLightInfos : register(t9);

uint GetDepthSliceIdx(float ViewZ)
{
    float BottomValue = 1 / log(FarClip / NearClip);
    ViewZ = clamp(ViewZ, NearClip, FarClip);
    return uint(floor(log(ViewZ) * ClusterSliceNumZ * BottomValue - ClusterSliceNumZ * log(NearClip) * BottomValue));
}

uint GetLightIndicesOffset(float3 WorldPos)
{
    float4 ViewPos = mul(float4(WorldPos, 1), View);
    float4 NDC = mul(ViewPos, Projection);
    NDC.xy /= NDC.w;
    //-1 ~ 1 =>0 ~ ScreenXSlideNum
    //-1 ~ 1 =>0 ~ ScreenYSlideNum
    //Near ~ Far => 0 ~ ZSlideNum
    float2 ScreenNorm = saturate(NDC.xy * 0.5f + 0.5f);
    uint2 ClusterXY = uint2(floor(ScreenNorm * float2(ClusterSliceNumX, ClusterSliceNumY)));
    uint ClusterZ = GetDepthSliceIdx(ViewPos.z);
    
    uint ClusterIdx = ClusterXY.x + ClusterXY.y * ClusterSliceNumX + ClusterSliceNumX * ClusterSliceNumY * ClusterZ;
    
    return LightMaxCountPerCluster * ClusterIdx;
}

uint GetPointLightCount(uint LightIndicesOffset)
{
    uint Count = 0;
    for (uint i = 0; i < LightMaxCountPerCluster; i++)
    {
        if (PointLightIndices[LightIndicesOffset + i] >= 0)
        {
            Count++;
        }
    }
    return Count;
}

uint GetPointLightIndex(uint LightIndicesIndex)
{
    return PointLightIndices[LightIndicesIndex];
}

FPointLightInfo GetPointLight(uint LightIdx)
{
    //uint LightInfoIdx = PointLightIndices[LightIdx];
    return PointLightInfos[LightIdx];
}

uint GetSpotLightCount(uint LightIndicesOffset)
{
    uint Count = 0;
    for (uint i = 0; i < LightMaxCountPerCluster; i++)
    {
        if (SpotLightIndices[LightIndicesOffset + i] >= 0)
        {
            Count++;
        }
    }
    return Count;
}

uint GetSpotLightIndex(uint LightIndicesIndex)
{
    return SpotLightIndices[LightIndicesIndex];
}

FSpotLightInfo GetSpotLight(uint LightIdx)
{
    // uint LightInfoIdx = SpotLightIndices[LightIdx];
    return SpotLightInfos[LightIdx];
}

// Textures
Texture2D DiffuseTexture : register(t0);
Texture2D AmbientTexture : register(t1);
Texture2D SpecularTexture : register(t2);
Texture2D NormalTexture : register(t3);
Texture2D AlphaTexture : register(t4);
Texture2D BumpTexture : register(t5);

// Shadow Maps
Texture2D ShadowAtlas : register(t10);
Texture2D VarianceShadowAtlas : register(t11);
StructuredBuffer<FShadowAtlasTilePos> ShadowAtlasDirectionalLightTilePos : register(t12);
StructuredBuffer<FShadowAtlasTilePos> ShadowAtlasSpotLightTilePos : register(t13);
StructuredBuffer<FShadowAtlasPointLightTilePos> ShadowAtlasPointLightTilePos : register(t14);

SamplerState SamplerWrap : register(s0);
SamplerComparisonState ShadowSampler : register(s1);
// SamplerState VarianceShadowSampler : register(s2);

// Material flags
#define HAS_DIFFUSE_MAP  (1 << 0) // map_Kd
#define HAS_AMBIENT_MAP  (1 << 1) // map_Ka
#define HAS_SPECULAR_MAP (1 << 2) // map_Ks
#define HAS_NORMAL_MAP   (1 << 3) // map_normal
#define HAS_ALPHA_MAP    (1 << 4) // map_d
#define HAS_BUMP_MAP     (1 << 5) // map_Bump

// Vertex Shader Input/Output
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
    float3 WorldNormal : TEXCOORD1;
    float2 Tex : TEXCOORD2;
    float4 WorldTangent : TEXCOORD3;
#if LIGHTING_MODEL_GOURAUD
    float4 AmbientLight : COLOR0;
    float4 DiffuseLight : COLOR1;
    float4 SpecularLight : COLOR2;
#endif
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

// Safe Normalize Util Functions
float2 SafeNormalize2(float2 v)
{
    float Len2 = dot(v, v);
    return Len2 > 1e-12f ? v / sqrt(Len2) : float2(0.0f, 0.0f);
}

float3 SafeNormalize3(float3 v)
{
    float Len2 = dot(v, v);
    return Len2 > 1e-12f ? v / sqrt(Len2) : float3(0.0f, 0.0f, 0.0f);
}

float4 SafeNormalize4(float4 v)
{
    float Len2 = dot(v, v);
    return Len2 > 1e-12f ? v / sqrt(Len2) : float4(0.0f, 0.0f, 0.0f, 0.0f);
}

float GetDeterminant3x3(float3x3 M)
{
    return M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
         - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0])
         + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
}

// 3x3 Matrix Inverse
// Returns identity matrix if determinant is near zero
float3x3 Inverse3x3(float3x3 M)
{
    float det = GetDeterminant3x3(M);
    
    // Singular matrix guard
    if (abs(det) < 1e-8)
        return float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1); // Identity matrix
    
    float invDet = 1.0f / det;
    
    float3x3 inv;
    inv[0][0] = (M[1][1] * M[2][2] - M[1][2] * M[2][1]) * invDet;
    inv[0][1] = (M[0][2] * M[2][1] - M[0][1] * M[2][2]) * invDet;
    inv[0][2] = (M[0][1] * M[1][2] - M[0][2] * M[1][1]) * invDet;
    
    inv[1][0] = (M[1][2] * M[2][0] - M[1][0] * M[2][2]) * invDet;
    inv[1][1] = (M[0][0] * M[2][2] - M[0][2] * M[2][0]) * invDet;
    inv[1][2] = (M[0][2] * M[1][0] - M[0][0] * M[1][2]) * invDet;
    
    inv[2][0] = (M[1][0] * M[2][1] - M[1][1] * M[2][0]) * invDet;
    inv[2][1] = (M[0][1] * M[2][0] - M[0][0] * M[2][1]) * invDet;
    inv[2][2] = (M[0][0] * M[1][1] - M[0][1] * M[1][0]) * invDet;
    
    return inv;
}

/*-----------------------------------------------------------------------------
    PCF (Percentage Closer Filtering)
 -----------------------------------------------------------------------------*/

float CalculatePercentageCloserShadowFactor(
    uint CastShadow,
    float4x4 LightViewProjection,
    float3 WorldPos,
    float Resolution,
    Texture2D AtlasTexture,
    uint2 AtlasUV
    )
{
    // If shadow is disabled, return fully lit (1.0)
    if (CastShadow == 0)
        return 1.0f;

    // Transform world position to light space
    float4 LightSpacePos = mul(float4(WorldPos, 1.0f), LightViewProjection);

    // Perspective divide (to NDC space)
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

    // 3x3 PCF kernel
    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        [unroll]
        for (int y = -1; y <= 1; y++)
        {
            float TexelSize = 1.0f / Resolution;
            float2 Offset = float2(x, y) * TexelSize;

            // 현재는 Directional Light가 한장이라고 가정하여
            // Shadow Atlas의 우측 상단을 가져온다고 가정
            // 추후 Cascade를 구현할 때 Atlas에서 Frustum Slice를 읽어올 수 있도록
            // 별도로 위치를 지정해야 함.
            float2 AtlasTexcoord =
                (ShadowTexCoord + Offset) * GET_TILE_SIZE(Resolution) + AtlasUV * ATLASGRIDSIZE;
            
            ShadowFactor += AtlasTexture.SampleCmpLevelZero(
                ShadowSampler,
                AtlasTexcoord,
                CurrentDepth
            );
        }
    }

    ShadowFactor /= 9.0f; // Average of 9 samples

    return ShadowFactor;
}

float GetCameraViewZ(float3 WorldPos)
{
    // 거리에 따라 알맞은 절두체를 선택한다.
    return mul(float4(WorldPos, 1.0f), View).z;
}

uint GetCascadeSubFrustumNum(float CameraViewZ)
{
    int SubFrustumNum;
    for (SubFrustumNum = 0; SubFrustumNum < SplitNum; SubFrustumNum++)
    {
        if (CameraViewZ <= SplitDistance[SubFrustumNum].r)
            break;
    }

    return SubFrustumNum;
}

// Shadow Calculation Functions
float CalculateDirectionalShadowFactor(FDirectionalLightInfo Light, float3 WorldPos)
{
    float CameraViewZ = GetCameraViewZ(WorldPos);
    uint SubFrustumNum = GetCascadeSubFrustumNum(CameraViewZ);

    float ShadowFactorFromSubFrustum =
        CalculatePercentageCloserShadowFactor(
            Light.CastShadow,
            mul(CascadeView, CascadeProj[SubFrustumNum]),
            WorldPos,
            Light.Resolution,
            ShadowAtlas,
            ShadowAtlasDirectionalLightTilePos[SubFrustumNum].UV
        );

    // 첫번째 SubFrustum이면 그대로 값을 반환
    if (SubFrustumNum == 0)
        return ShadowFactorFromSubFrustum;

    float OverlappedSectionMin = SplitDistance[SubFrustumNum - 1].r;
    float OverlappedSectionMax = OverlappedSectionMin * 1.1f;
    
    if (CameraViewZ <= OverlappedSectionMax)
    {
        float ShadowFactorFromSubFrustum2 =
        CalculatePercentageCloserShadowFactor(
            Light.CastShadow,
            mul(CascadeView, CascadeProj[SubFrustumNum - 1]),
            WorldPos,
            Light.Resolution,
            ShadowAtlas,
            ShadowAtlasDirectionalLightTilePos[SubFrustumNum - 1].UV
        );

        float t = (CameraViewZ - OverlappedSectionMin) /
            (OverlappedSectionMax - OverlappedSectionMin);

        return t * ShadowFactorFromSubFrustum + (1.0f - t) * ShadowFactorFromSubFrustum2;
    }
    else
    {
        return ShadowFactorFromSubFrustum;
    }
}

float CalculateSpotShadowFactor(
    FSpotLightInfo Light,
    uint LightIndex,
    float3 WorldPos
    )
{
    // Light의 개수가 규정된 상한을 초과하면 그림자 계산 스킵
    if (LightIndex >= MAX_SPOTLIGHT_NUM)
    {
        return 1.0f;
    }

    return CalculatePercentageCloserShadowFactor(
        Light.CastShadow,
        Light.LightViewProjection,
        WorldPos,
        Light.Resolution,
        ShadowAtlas,
        ShadowAtlasSpotLightTilePos[LightIndex].UV
    );
}

// Point Light PCF (Percentage Closer Filtering) Sample Offsets
// 20 samples in 3D space for soft shadows on cube maps
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

    // 2-direction 3D diagonal offsets (corners of cube)
    float3( 1.0,  1.0,  1.0), float3(-1.0, -1.0, -1.0)
};

float2 GetPointLightShadowMapUVWithDirection(
    float3 Direction,
    uint LightIndex,
    float Resolution
    )
{
    float3 AbsDir = abs(Direction);
    float2 UV;
    uint FaceIndex;

    // Major axis selection (DirectX standard)
    if (AbsDir.x >= AbsDir.y && AbsDir.x >= AbsDir.z)
    {
      // X-axis dominant
      if (Direction.x > 0.0f)  // +X face
      {
          FaceIndex = 0;
          float ma = AbsDir.x;
          float sc = -Direction.z;
          float tc = -Direction.y;
          UV.x = (sc / ma + 1.0f) * 0.5f;
          UV.y = (tc / ma + 1.0f) * 0.5f;
      }
      else  // -X face
      {
          FaceIndex = 1;
          float ma = AbsDir.x;
          float sc = Direction.z;
          float tc = -Direction.y;
          UV.x = (sc / ma + 1.0f) * 0.5f;
          UV.y = (tc / ma + 1.0f) * 0.5f;
      }
    }
    else if (AbsDir.y >= AbsDir.z)
    {
      // Y-axis dominant
      if (Direction.y > 0.0f)  // +Y face
      {
          FaceIndex = 2;
          float ma = AbsDir.y;
          float sc = Direction.x;
          float tc = Direction.z;
          UV.x = (sc / ma + 1.0f) * 0.5f;
          UV.y = (tc / ma + 1.0f) * 0.5f;
      }
      else  // -Y face
      {
          FaceIndex = 3;
          float ma = AbsDir.y;
          float sc = Direction.x;
          float tc = -Direction.z;
          UV.x = (sc / ma + 1.0f) * 0.5f;
          UV.y = (tc / ma + 1.0f) * 0.5f;
      }
    }
    else
    {
      // Z-axis dominant
      if (Direction.z > 0.0f)  // +Z face
      {
          FaceIndex = 4;
          float ma = AbsDir.z;
          float sc = Direction.x;
          float tc = -Direction.y;
          UV.x = (sc / ma + 1.0f) * 0.5f;
          UV.y = (tc / ma + 1.0f) * 0.5f;
      }
      else  // -Z face
      {
          FaceIndex = 5;
          float ma = AbsDir.z;
          float sc = -Direction.x;
          float tc = -Direction.y;
          UV.x = (sc / ma + 1.0f) * 0.5f;
          UV.y = (tc / ma + 1.0f) * 0.5f;
      }
    }

    // Atlas UV 계산
    float2 AtlasUV = UV * GET_TILE_SIZE(Resolution);

    FShadowAtlasPointLightTilePos AtlasTilePos = ShadowAtlasPointLightTilePos[LightIndex];
    AtlasUV.x += ATLASGRIDSIZE * AtlasTilePos.UV[FaceIndex].x;
    AtlasUV.y += ATLASGRIDSIZE * AtlasTilePos.UV[FaceIndex].y;

    return AtlasUV;
}

float CalculatePointShadowFactor(FPointLightInfo Light, uint LightIndex ,float3 WorldPos)
{
    // If shadow is disabled, return fully lit (1.0)
    if (Light.CastShadow == 0)
        return 1.0f;

    // Light의 개수가 규정된 상한을 초과하면 그림자 계산 스킵
    if (LightIndex >= MAX_POINT_LIGHT_NUM)
        return 1.0f;

    // Calculate direction from light to pixel (for cube map sampling)
    float3 LightToPixel = WorldPos - Light.Position;
    float Distance = length(LightToPixel);

    // If too close to light or outside range, assume lit
    if (Distance < 1e-6 || Distance > Light.Range)
        return 1.0f;

    // Normalize direction for cube sampling
    float3 SampleDir = LightToPixel / Distance;

    // Convert current distance to normalized [0,1] range
    float CurrentDistance = Distance / Light.Range;

    // Add small bias to prevent shadow acne
    float Bias = 0.005f;

    // PCF (Percentage Closer Filtering) for soft shadows
    // Filter radius controls shadow softness (larger = softer)
    float FilterRadius = 0.003f;

    float ShadowFactor = 0.0f;
    float TotalSamples = 1.0f;  // Start with 1 for center sample
    
    float2 AtlasTexcoord = GetPointLightShadowMapUVWithDirection(SampleDir, LightIndex, Light.Resolution);
    
    //float StoredDistance = ShadowAtlas.Load(int3(AtlasTexcoord, 0)).r;
    float StoredDistance = ShadowAtlas.Sample(SamplerWrap, AtlasTexcoord).r;

    ShadowFactor += (CurrentDistance - Bias) > StoredDistance ? 0.0f : 1.0f;

    [unroll]
    for (int i = 0; i < 20; i++)
    {
        // Apply offset in tangent space around the sample direction
        float3 OffsetDir = normalize(SampleDir + CubePCFOffsets[i] * FilterRadius);
        AtlasTexcoord = GetPointLightShadowMapUVWithDirection(OffsetDir, LightIndex, Light.Resolution);
        
        // Sample shadow map with offset direction
        StoredDistance = ShadowAtlas.Sample(SamplerWrap, AtlasTexcoord).r;

        // Compare and accumulate
        ShadowFactor += (CurrentDistance - Bias) > StoredDistance ? 0.0f : 1.0f;
        TotalSamples += 1.0f;
    }

    // Average all samples
    return ShadowFactor / TotalSamples;
}

/*-----------------------------------------------------------------------------
    VSM (Variance Shadow Map) Filtering
 -----------------------------------------------------------------------------*/

/**
 * @brief VSM/SAVSM 공통 계산: 체비쇼프 부등식을 사용하여 그림자 인자를 계산합니다.
 * @param CurrentDepth 픽셀의 현재 깊이 (라이트 공간)
 * @param Moments 샘플링된 모멘트 (float2(E(X), E(X^2)))
 * @return float ShadowFactor (0.0 = shadowed, 1.0 = lit)
 */
float CalculateChebyshevShadowFactor(float CurrentDepth, float2 Moments)
{
    // First Moment: E(X)
    float M1 = Moments.x;

    // Second Moment: E(X^2)
    float M2 = Moments.y;

    // Variance (max for numerical stability)
    float Variance = max(0.00001f, M2 - M1 * M1);

    // If current depth is smaller than first moment, return 1.0f (Light Bleeding 방지)
    if (CurrentDepth <= M1)
    {
        return 1.0f;
    }

    // Chebyshev's Inequality
    // P(X > t) <= P_max = Var(X) / (Var(X) + (t - E(X))^2)
    float Difference = CurrentDepth - M1;

    // P_max
    float ShadowFactor = Variance / (Variance + Difference * Difference);

    return saturate(ShadowFactor); 
}

float CalculateVarianceShadowFactor(
    uint CastShadow,
    float4x4 LightViewProjection,
    float3 WorldPos,
    Texture2D AtlasTexture,
    uint2 AtlasUV,
    float Resolution
    )
{
    // If shadow is disabled, return fully lit (1.0)
    if (CastShadow == 0)
    {
        return 1.0f;
    }

    // --- 1. 월드 -> 라이트 공간 -> UV 변환 ---
    float4 LightSpacePos = mul(float4(WorldPos, 1.0f), LightViewProjection);
    LightSpacePos.xyz /= LightSpacePos.w;

    if (LightSpacePos.x < -1.0f || LightSpacePos.x > 1.0f ||
        LightSpacePos.y < -1.0f || LightSpacePos.y > 1.0f ||
        LightSpacePos.z < 0.0f || LightSpacePos.z > 1.0f)
    {
        return 1.0f; // Outside shadow map, assume lit
    }

    float2 ShadowTexCoord;
    ShadowTexCoord.x = LightSpacePos.x * 0.5f + 0.5f;
    ShadowTexCoord.y = -LightSpacePos.y * 0.5f + 0.5f; // Flip Y for texture space

    float2 AtlasTexCoord = ShadowTexCoord * GET_TILE_SIZE(Resolution);
    AtlasTexCoord += ATLASGRIDSIZE * AtlasUV;

    float CurrentDepth = LightSpacePos.z;

    // --- 2. VSM 샘플링 ---
    float2 Moments = AtlasTexture.Sample(SamplerWrap, AtlasTexCoord);

    // --- 3. 공통 체비쇼프 계산 ---
    return CalculateChebyshevShadowFactor(CurrentDepth, Moments);
}

float CalculateDirectionalVarianceShadowFactor(FDirectionalLightInfo Light, float3 WorldPos)
{
    float CameraViewZ = GetCameraViewZ(WorldPos);
    uint SubFrustumNum = GetCascadeSubFrustumNum(CameraViewZ);

    float ShadowFactorFromSubFrustum =
        CalculateVarianceShadowFactor(
            Light.CastShadow,
            mul(CascadeView, CascadeProj[SubFrustumNum]),
            WorldPos,
            VarianceShadowAtlas,
            ShadowAtlasDirectionalLightTilePos[SubFrustumNum].UV,
            Light.Resolution
        );

    // 첫번째 SubFrustum이면 그대로 값을 반환
    if (SubFrustumNum == 0)
        return ShadowFactorFromSubFrustum;

    float OverlappedSectionMin = SplitDistance[SubFrustumNum - 1].r;
    float OverlappedSectionMax = OverlappedSectionMin * 1.1f;
    
    if (CameraViewZ <= OverlappedSectionMax)
    {
        float ShadowFactorFromSubFrustum2 =
        CalculateVarianceShadowFactor(
            Light.CastShadow,
            mul(CascadeView, CascadeProj[SubFrustumNum - 1]),
            WorldPos,
            VarianceShadowAtlas,
            ShadowAtlasDirectionalLightTilePos[SubFrustumNum - 1].UV,
            Light.Resolution
        );

        float t = (CameraViewZ - OverlappedSectionMin) /
            (OverlappedSectionMax - OverlappedSectionMin);

        return t * ShadowFactorFromSubFrustum + (1.0f - t) * ShadowFactorFromSubFrustum2;
    }
    else
    {
        return ShadowFactorFromSubFrustum;
    }
}

float CalculateSpotVarianceShadowFactor(FSpotLightInfo Light, uint LightIndex, float3 WorldPos)
{
    // Light의 개수가 규정된 상한을 초과하면 그림자 계산 스킵
    if (LightIndex >= MAX_SPOTLIGHT_NUM)
    {
        return 1.0f;
    }

    return CalculateVarianceShadowFactor(
        Light.CastShadow,
        Light.LightViewProjection,
        WorldPos,
        VarianceShadowAtlas,
        ShadowAtlasSpotLightTilePos[LightIndex].UV,
        Light.Resolution
    );
}

/*-----------------------------------------------------------------------------
    SAVSM (Summed Area Variance Shadow Map) Filtering
 -----------------------------------------------------------------------------*/

/**
 * @brief Summed Area Table(SAT)을 쿼리하여 사각 영역의 평균 모멘트를 계산합니다.
 * @param AtlasTexture 쿼리할 Summed Area Variance Shadow Map
 * @param AtlasUV Atlas의 인덱스 (UV 좌표 아님) 
 * @param CenterUV 필터링할 영역의 중심 UV 좌표
 * @param FilterRadiusUV 필터링할 영역의 반지름 (UV 공간)
 * @param TexelSize 텍스처 1픽셀의 UV 크기 (1.0f / 텍스처 크기)
 * @return float2(Average E(X), Average E(X^2))
 */
float2 SampleSummedAreaVarianceShadowMap(
    Texture2D AtlasTexture,
    uint2 AtlasUV,
    float2 CenterUV,
    float2 FilterRadiusUV,
    float Resolution
    )
{
    // 1. 필터링할 영역의 사각 경계(UV)를 계산
    float2 UV_TopRight = CenterUV + FilterRadiusUV;
    float2 UV_BottomLeft = CenterUV - FilterRadiusUV;

    // 텍스처 크기 및 텍셀 크기 계산
    float2 TexelSize = float2(1.0f / Resolution, 1.0f / Resolution);
    
    // 2. SAT(Summed Area Table) 쿼리를 위해서 4개의 샘플링 좌표를 계산
    // SAT(x, y) = [0, 0]부터 [x, y]까지의 합
    float2 UV_A = float2(UV_BottomLeft.x - TexelSize.x, UV_BottomLeft.y - TexelSize.y); // A = (x1 -1, y1 -1)
    float2 UV_B = float2(UV_TopRight.x, UV_BottomLeft.y - TexelSize.y);                 // B = (x2, y1 - 1)
    float2 UV_C = float2(UV_BottomLeft.x - TexelSize.x, UV_TopRight.y);                 // C = (x1 - 1, y2)
    float2 UV_D = float2(UV_TopRight.x, UV_TopRight.y);                                 // D = (x2, y2)

    float2 AtlasUV_A = UV_A * GET_TILE_SIZE(Resolution) + AtlasUV * ATLASGRIDSIZE;
    float2 AtlasUV_B = UV_B * GET_TILE_SIZE(Resolution) + AtlasUV * ATLASGRIDSIZE;
    float2 AtlasUV_C = UV_C * GET_TILE_SIZE(Resolution) + AtlasUV * ATLASGRIDSIZE;
    float2 AtlasUV_D = UV_D * GET_TILE_SIZE(Resolution) + AtlasUV * ATLASGRIDSIZE;

    // 3. SAT 텍스쳐 샘플링
    // TODO: SamplerClamp 또는 SamplerBorder 대체
    float2 Moments_A = AtlasTexture.SampleLevel(SamplerWrap, AtlasUV_A, 0);
    float2 Moments_B = AtlasTexture.SampleLevel(SamplerWrap, AtlasUV_B, 0);
    float2 Moments_C = AtlasTexture.SampleLevel(SamplerWrap, AtlasUV_C, 0);
    float2 Moments_D = AtlasTexture.SampleLevel(SamplerWrap, AtlasUV_D, 0);

    // 4. 포함-배제 원칙 (Inclusion-Exclusion)
    float2 SummedMoments = Moments_D - Moments_B - Moments_C + Moments_A;

    // 5. 필터 영역의 넓이를 계산 (UV 공간 -> 픽셀 공간)
    // 필터 반경 R인 커널은 (2R+1) x (2R+1) 크기를 가집니다.
    float2 SideLengths = (FilterRadiusUV / TexelSize) * 2.0f + 1.0f;
    float FilterArea = SideLengths.x * SideLengths.y;

    if (FilterArea < 1.0f)
    {
        FilterArea = 1.0f; // 0으로 나누는 것을 방지
    }

    // 6. 합계를 넓이로 나누어 평균 모멘트 계산
    return SummedMoments / FilterArea;
}

float CalculateSummedAreaVarianceShadowFactor(
    uint CastShadow,
    float4x4 LightViewProjection,
    float3 WorldPos,
    Texture2D AtlasTexture,
    uint2 AtlasUV,
    float Resolution
    )
{
    // If shadow is disabled, return fully lit (1.0)
    if (CastShadow == 0)
    {
        return 1.0f;
    }

    // --- 1. 월드 -> 라이트 공간 -> UV 변환 (VSM과 동일) ---
    float4 LightSpacePos = mul(float4(WorldPos, 1.0f), LightViewProjection);
    LightSpacePos.xyz /= LightSpacePos.w;

    if (LightSpacePos.x < -1.0f || LightSpacePos.x > 1.0f ||
        LightSpacePos.y < -1.0f || LightSpacePos.y > 1.0f ||
        LightSpacePos.z < 0.0f || LightSpacePos.z > 1.0f)
    {
        return 1.0f; // Outside shadow map, assume lit
    }

    float2 ShadowTexCoord;
    ShadowTexCoord.x = LightSpacePos.x * 0.5f + 0.5f;
    ShadowTexCoord.y = -LightSpacePos.y * 0.5f + 0.5f; // Flip Y

    float CurrentDepth = LightSpacePos.z;

    // --- 2. SAVSM 샘플링 ---
    
    // 텍스처 크기 및 텍셀 크기 계산
    float2 TexelSize = float2(1.0f / Resolution, 1.0f / Resolution);

    // 필터 커널 크기 정의 (예: 4x4 픽셀 반경)
    float KernelRadiusPixels = 4.0f;
    float2 FilterRadiusUV = KernelRadiusPixels * TexelSize;

    float2 AverageMoments = SampleSummedAreaVarianceShadowMap(
        AtlasTexture,
        AtlasUV,
        ShadowTexCoord,
        FilterRadiusUV,
        Resolution
    );

    // --- 3. 공통 체비쇼프 계산 ---
    return CalculateChebyshevShadowFactor(CurrentDepth, AverageMoments);
}

float CalculateDirectionalSummedAreaVarianceShadowFactor(FDirectionalLightInfo Light, float3 WorldPos)
{
    float CameraViewZ = GetCameraViewZ(WorldPos);
    uint SubFrustumNum = GetCascadeSubFrustumNum(CameraViewZ);

    float ShadowFactorFromSubFrustum =
        CalculateSummedAreaVarianceShadowFactor(
            Light.CastShadow,
            mul(CascadeView, CascadeProj[SubFrustumNum]),
            WorldPos,
            VarianceShadowAtlas,
            ShadowAtlasDirectionalLightTilePos[SubFrustumNum].UV,
            Light.Resolution
        );

    // 첫번째 SubFrustum이면 그대로 값을 반환
    if (SubFrustumNum == 0)
        return ShadowFactorFromSubFrustum;

    float OverlappedSectionMin = SplitDistance[SubFrustumNum - 1].r;
    float OverlappedSectionMax = OverlappedSectionMin * 1.1f;
    
    if (CameraViewZ <= OverlappedSectionMax)
    {
        float ShadowFactorFromSubFrustum2 =
        CalculateSummedAreaVarianceShadowFactor(
            Light.CastShadow,
            mul(CascadeView, CascadeProj[SubFrustumNum - 1]),
            WorldPos,
            VarianceShadowAtlas,
            ShadowAtlasDirectionalLightTilePos[SubFrustumNum - 1].UV,
            Light.Resolution
        );

        float t = (CameraViewZ - OverlappedSectionMin) /
            (OverlappedSectionMax - OverlappedSectionMin);

        return t * ShadowFactorFromSubFrustum + (1.0f - t) * ShadowFactorFromSubFrustum2;
    }
    else
    {
        return ShadowFactorFromSubFrustum;
    }
}

float CalculateSpotSummedAreaVarianceShadowFactor(FSpotLightInfo Light, uint LightIndex, float3 WorldPos)
{
    // Light의 개수가 규정된 상한을 초과하면 그림자 계산 스킵
    if (LightIndex >= MAX_SPOTLIGHT_NUM)
    {
        return 1.0f;
    }

    return CalculateSummedAreaVarianceShadowFactor(
        Light.CastShadow,
        Light.LightViewProjection,
        WorldPos,
        VarianceShadowAtlas,
        ShadowAtlasSpotLightTilePos[LightIndex].UV,
        Light.Resolution
    );
}

/*-----------------------------------------------------------------------------
    Lighting Calculation Functions
 -----------------------------------------------------------------------------*/

float4 CalculateAmbientLight(FAmbientLightInfo info)
{
    return info.Color * info.Intensity;
}

FIllumination CalculateDirectionalLight(FDirectionalLightInfo Info, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    FIllumination Result = (FIllumination) 0;

    // LightDir 또는 WorldNormal이 영벡터면 결과도 전부 영벡터가 되므로 계산 종료 (Nan 방어 코드도 겸함)
    if (dot(Info.Direction, Info.Direction) < 1e-12 || dot(WorldNormal, WorldNormal) < 1e-12)
        return Result;

    float3 LightDir = SafeNormalize3(-Info.Direction);
    float NdotL = saturate(dot(WorldNormal, LightDir));

    // Calculate shadow factor (0 = shadow, 1 = lit)
    float ShadowFactor = 1.0f;
    if (Info.ShadowModeIndex == SMI_PCF)
    {
        ShadowFactor = CalculateDirectionalShadowFactor(Info, WorldPos);
    }
    else if (Info.ShadowModeIndex == SMI_VSM || Info.ShadowModeIndex == SMI_VSM_BOX || Info.ShadowModeIndex == SMI_VSM_GAUSSIAN)
    {
        ShadowFactor = CalculateDirectionalVarianceShadowFactor(Info, WorldPos);
    }
    else if (Info.ShadowModeIndex == SMI_SAVSM)
    {
        ShadowFactor = CalculateDirectionalSummedAreaVarianceShadowFactor(Info, WorldPos);
    }

    // diffuse illumination (affected by shadow)
    Result.Diffuse = Info.Color * Info.Intensity * NdotL * ShadowFactor;

#if LIGHTING_MODEL_BLINNPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong) - also affected by shadow
    float3 WorldToCameraVector = SafeNormalize3(ViewPos - WorldPos); // 영벡터면 결과적으로 LightDir와 같은 셈이 됨
    float3 WorldToLightVector = LightDir;

    float3 H = SafeNormalize3(WorldToLightVector + WorldToCameraVector); // H가 영벡터면 Specular도 영벡터
    float CosTheta = saturate(dot(WorldNormal, H));
    float Spec = CosTheta < 1e-6 ? 0.0f : ((Ns + 8.0f) / (8.0f * PI)) * pow(CosTheta, Ns); // 0^0 방지를 위해 이렇게 계산함
    Result.Specular = Info.Color * Info.Intensity * Spec * ShadowFactor;
#endif

    return Result;
}

FIllumination CalculatePointLight(FPointLightInfo Info, uint LightIndex, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    FIllumination Result = (FIllumination) 0;

    float3 LightDir = Info.Position - WorldPos;
    float Distance = length(LightDir);

    // 거리나 범위가 너무 작거나, 거리가 범위 밖이면 조명 기여 없음 (Nan 방어 코드도 겸함)
    if (Distance < 1e-6 || Info.Range < 1e-6 || Distance > Info.Range)
        return Result;

    LightDir = SafeNormalize3(LightDir);
    float NdotL = saturate(dot(WorldNormal, LightDir));
    // attenuation based on distance: (1 - d / R)^n
    float Attenuation = pow(saturate(1.0f - Distance / Info.Range), Info.DistanceFalloffExponent);

    // Calculate shadow factor (0 = shadow, 1 = lit)
    float ShadowFactor = CalculatePointShadowFactor(Info, LightIndex, WorldPos);

    // diffuse illumination (affected by shadow)
    Result.Diffuse = Info.Color * Info.Intensity * NdotL * Attenuation * ShadowFactor;

#if LIGHTING_MODEL_BLINNPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong) - also affected by shadow
    float3 WorldToCameraVector = SafeNormalize3(ViewPos - WorldPos); // 영벡터면 결과적으로 LightDir와 같은 셈이 됨
    float3 WorldToLightVector = LightDir;

    float3 H = SafeNormalize3(WorldToLightVector + WorldToCameraVector); // H가 영벡터면 Specular도 영벡터
    float CosTheta = saturate(dot(WorldNormal, H));
    float Spec = CosTheta < 1e-6 ? 0.0f : ((Ns + 8.0f) / (8.0f * PI)) * pow(CosTheta, Ns); // 0^0 방지를 위해 이렇게 계산함
    Result.Specular = Info.Color * Info.Intensity * Spec * Attenuation * ShadowFactor;
#endif

    return Result;
}

FIllumination CalculateSpotLight(FSpotLightInfo Info, uint LightIndex, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    FIllumination Result = (FIllumination) 0;
    
    float3 LightDir = Info.Position - WorldPos;
    float Distance = length(LightDir);
    
    // 거리나 범위가 너무 작거나, 거리가 범위 밖이면 조명 기여 없음 (Nan 방어 코드도 겸함)
    if (Distance < 1e-6 || Info.Range < 1e-6 || Distance > Info.Range)
        return Result;
    
    LightDir = SafeNormalize3(LightDir);
    float3 SpotDir = SafeNormalize3(Info.Direction); // SpotDIr이 영벡터면 (CosAngle < CosOuter)에 걸려 0벡터 반환
    
    float CosAngle = dot(-LightDir, SpotDir);
    float CosOuter = cos(Info.OuterConeAngle);
    float CosInner = cos(Info.InnerConeAngle);
    if (CosAngle - CosOuter <= 1e-6)
        return Result;
    
    float NdotL = saturate(dot(WorldNormal, LightDir));

    // attenuation based on distance: (1 - d / R)^n
    float AttenuationDistance = pow(saturate(1.0f - Distance / Info.Range), Info.DistanceFalloffExponent);

    float AttenuationAngle = 0.0f;
    if (CosAngle >= CosInner || CosInner - CosOuter <= 1e-6)
    {
        AttenuationAngle = 1.0f;
    }
    else
    {
        AttenuationAngle = pow(saturate((CosAngle - CosOuter) / (CosInner - CosOuter)), Info.AngleFalloffExponent);
    }

    // Shadow factor
    float ShadowFactor = 1.0f;
    if (Info.ShadowModeIndex == SMI_PCF)
    {
        ShadowFactor = CalculateSpotShadowFactor(Info, LightIndex, WorldPos);
    }
    else if (Info.ShadowModeIndex == SMI_VSM || Info.ShadowModeIndex == SMI_VSM_BOX || Info.ShadowModeIndex == SMI_VSM_GAUSSIAN)
    {
        ShadowFactor = CalculateSpotVarianceShadowFactor(Info, LightIndex, WorldPos);
    }
    else if (Info.ShadowModeIndex == SMI_SAVSM)
    {
        ShadowFactor = CalculateSpotSummedAreaVarianceShadowFactor(Info, LightIndex, WorldPos);
    }

    Result.Diffuse = Info.Color * Info.Intensity * NdotL * AttenuationDistance * AttenuationAngle * ShadowFactor;

#if LIGHTING_MODEL_BLINNPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong)
    float3 WorldToCameraVector = SafeNormalize3(ViewPos - WorldPos);
    float3 WorldToLightVector = LightDir;

    float3 H = SafeNormalize3(WorldToLightVector + WorldToCameraVector); // H가 영벡터면 Specular도 영벡터
    float CosTheta = saturate(dot(WorldNormal, H));
    float Spec = CosTheta < 1e-6 ? 0.0f : ((Ns + 8.0f) / (8.0f * PI)) * pow(CosTheta, Ns); // 0^0 방지를 위해 이렇게 계산함
    Result.Specular = Info.Color * Info.Intensity * Spec * AttenuationDistance * AttenuationAngle * ShadowFactor;
#endif
    
    return Result;
}

float3 ComputeNormalMappedWorldNormal(float2 UV, float3 WorldNormal, float4 WorldTangent)
{
    float3 BaseNormal = SafeNormalize3(WorldNormal);

    // Tangent가 비정상(0 길이)이면 노말 맵 적용 포기하고 메시 노말 사용
    float TangentLen2 = dot(WorldTangent.xyz, WorldTangent.xyz);
    if (TangentLen2 <= 1e-8f)
    {
        return BaseNormal;
    }
    // 노말 맵 텍셀 샘플링 [0,1]
    float3 Encoded = NormalTexture.Sample(SamplerWrap, UV).xyz;
    // [0,1] -> [-1,1]로 매핑해서 탄젠트 공간 노말을 복원한다.
    float3 TangentSpaceNormal = SafeNormalize3(Encoded * 2.0f - 1.0f);

    // VS로 넘어온 월드 탄젠트를 정규화
    float3 T = WorldTangent.xyz / sqrt(TangentLen2);
    // TBN이 올바른 방향이 되도록 저장해둔 좌우손성으로 B 복원
    float Handedness = WorldTangent.w;
    float3 B = SafeNormalize3(cross(BaseNormal, T) * Handedness);

    float3x3 TBN = float3x3(T, B, BaseNormal);
    // 로컬 공간의 탄젠트를 월드 공간으로 보냄
    return SafeNormalize3(mul(TangentSpaceNormal, TBN));

}
// Vertex Shader
PS_INPUT Uber_VS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
    float3x3 World3x3 = (float3x3) World;
    Output.WorldNormal = SafeNormalize3(mul(Input.Normal, transpose(Inverse3x3(World3x3))));
    float3 WorldTangent = SafeNormalize3(mul(Input.Tangent.xyz, (float3x3) World));
    Output.WorldTangent = float4(WorldTangent, Input.Tangent.w);
    Output.Tex = Input.Tex;
    
#if LIGHTING_MODEL_GOURAUD
    // Calculate lighting in vertex shader (Gouraud)
    // Accumulate light only; material and textures are applied in pixel stage
    FIllumination Illumination = (FIllumination)0;
    
    // 1. Ambient Light
    Illumination.Ambient = CalculateAmbientLight(Ambient);
    
    // 2. Directional Light
    ADD_ILLUM(Illumination, CalculateDirectionalLight(Directional, Output.WorldNormal, Output.WorldPosition, ViewWorldLocation))

    // 3. Point Lights
    uint LightIndicesOffset = GetLightIndicesOffset(Output.WorldPosition);
    uint PointLightCount = GetPointLightCount(LightIndicesOffset);
    [loop]
    for (uint i = 0; i < PointLightCount; i++)
    {
        uint LightIndex = GetPointLightIndex(LightIndicesOffset + i);
        FPointLightInfo PointLight = GetPointLight(LightIndex);
        ADD_ILLUM(Illumination, CalculatePointLight(PointLight, LightIndex, Output.WorldNormal, Output.WorldPosition, ViewWorldLocation));
    }

    // 4.Spot Lights
    uint SpotLightCount = GetSpotLightCount(LightIndicesOffset);
    [loop]
    for (uint j = 0; j < SpotLightCount; j++)
    {
        uint LightIndex = GetSpotLightIndex(LightIndicesOffset + j);
        FSpotLightInfo SpotLight = GetSpotLight(LightIndex);
        ADD_ILLUM(Illumination, CalculateSpotLight(SpotLight, LightIndex, Output.WorldNormal, Output.WorldPosition, ViewWorldLocation));
    }
    
    // Assign to output
    Output.AmbientLight = Illumination.Ambient;
    Output.DiffuseLight = Illumination.Diffuse;
    Output.SpecularLight = Illumination.Specular;
#endif
    
    return Output;
}

// Pixel Shader
PS_OUTPUT Uber_PS(PS_INPUT Input)
{
    PS_OUTPUT Output;
    float4 finalPixel = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float2 UV = Input.Tex;
    float3 ShadedWorldNormal = SafeNormalize3(Input.WorldNormal);
    if (MaterialFlags & HAS_NORMAL_MAP)
    {
        ShadedWorldNormal = ComputeNormalMappedWorldNormal(UV, Input.WorldNormal, Input.WorldTangent);
        // else: Tangent가 유효하지 않으면 NormalBase 유지
    }
    // Sample textures
    float4 ambientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        ambientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }
    else if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        // If no ambient map, but diffuse map exists, use diffuse map for ambient color
        ambientColor *= DiffuseTexture.Sample(SamplerWrap, UV);
    }
    
    float4 diffuseColor = Kd;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        diffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
    }
    
    float4 specularColor = Ks;
    if (MaterialFlags & HAS_SPECULAR_MAP)
    {
        specularColor *= SpecularTexture.Sample(SamplerWrap, UV);
    }
    
    
#if LIGHTING_MODEL_GOURAUD
    // Use pre-calculated vertex lighting; apply diffuse material/texture per-pixel
    finalPixel.rgb = Input.AmbientLight.rgb * ambientColor.rgb + Input.DiffuseLight.rgb * diffuseColor.rgb + Input.SpecularLight.rgb * specularColor.rgb;
    
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_BLINNPHONG
    // Calculate lighting in pixel shader
    FIllumination Illumination = (FIllumination)0;
    float3 N = ShadedWorldNormal;
    
    // 1. Ambient Light
    Illumination.Ambient = CalculateAmbientLight(Ambient);
    
    // 2. Directional Light
    ADD_ILLUM(Illumination, CalculateDirectionalLight(Directional, N, Input.WorldPosition, ViewWorldLocation));
    
    // 3. Point Lights
    uint LightIndicesOffset = GetLightIndicesOffset(Input.WorldPosition);
    uint PointLightCount = GetPointLightCount(LightIndicesOffset);
    [loop]
    for (uint i = 0; i < PointLightCount ; i++)
    {
        uint LightIndex = GetPointLightIndex(LightIndicesOffset + i);
        FPointLightInfo PointLight = GetPointLight(LightIndex);
        ADD_ILLUM(Illumination, CalculatePointLight(PointLight, LightIndex, N, Input.WorldPosition, ViewWorldLocation));
    }

    // 4. Spot Lights
    uint SpotLightCount = GetSpotLightCount(LightIndicesOffset);
    [loop]
    for (uint j = 0; j < SpotLightCount ; j++)
    {
        uint LightIndex = GetSpotLightIndex(LightIndicesOffset + j);
        FSpotLightInfo SpotLight = GetSpotLight(LightIndex);
        ADD_ILLUM(Illumination, CalculateSpotLight(SpotLight, LightIndex, N, Input.WorldPosition, ViewWorldLocation));
    }
    
    finalPixel.rgb = Illumination.Ambient.rgb * ambientColor.rgb + Illumination.Diffuse.rgb * diffuseColor.rgb + Illumination.Specular.rgb * specularColor.rgb;
    
#elif LIGHTING_MODEL_NORMAL
    float3 EncodedWorldNormal = ShadedWorldNormal * 0.5f + 0.5f;
    finalPixel.rgb = EncodedWorldNormal;
    
#else
    // Fallback: simple textured rendering (like current TexturePS.hlsl)
    finalPixel.rgb = diffuseColor.rgb + ambientColor.rgb;
#endif
    
    // Alpha handling
#if LIGHTING_MODEL_NORMAL
    finalPixel.a = 1.0f;
#else
    // 1. Diffuse Map 있으면 그 alpha 사용, 없으면 1.0
    float alpha = (MaterialFlags & HAS_DIFFUSE_MAP) ? diffuseColor.a : 1.0f;
    
    // 2. Alpha Map 따로 있으면 곱해줌.
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        alpha *= AlphaTexture.Sample(SamplerWrap, UV).r;
    }
    
    // 3. D 곱해서 최종 alpha 결정
    finalPixel.a = D * alpha;
#endif
    Output.SceneColor = finalPixel;
    
    // Encode normal for deferred rendering
    float3 encodedNormal = SafeNormalize3(ShadedWorldNormal) * 0.5f + 0.5f;
    Output.NormalData = float4(encodedNormal, 1.0f);
    
    return Output;
}
