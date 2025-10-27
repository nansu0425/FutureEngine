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