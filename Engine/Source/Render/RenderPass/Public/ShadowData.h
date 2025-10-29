#pragma once

struct FShadowViewProjConstant
{
    FMatrix ViewProjection;
};

// Point Light Shadow constant buffer (PointLightShadowDepth.hlsl의 b2)
struct FPointLightShadowParams
{
    FVector LightPosition;
    float LightRange;
};

struct FShadowAtlasTilePos
{
    uint32 UV[2];
    uint32 Padding[2];
};

struct FShadowAtlasPointLightTilePos
{
    uint32 UV[6][2];
};

enum class EPlaneVertexPos
{
    TOP_LEFT = 0,
    TOP_RIGHT = 1,
    BOTTOM_LEFT = 2,
    BOTTOM_RIGHT = 3
};

struct FCascadeSubFrustum
{
    FVector4 NearPlane[4];
    FVector4 SubFarPlane[8][4];
};

struct FCascadeShadowMapData
{
    // Reordered layout to match HLSL expectations
    FMatrix View;                        // 64 bytes
    FMatrix Proj[8];                     // 512 bytes
    FVector4 SplitDistance[8];         // 32 bytes (actual data)
    uint32 SplitNum = 0;                 // 4 bytes
    float BandingAreaFactor = 1.1f;
    uint32 Padding[2] = {};              // 12 bytes padding for 16-byte alignment
};
