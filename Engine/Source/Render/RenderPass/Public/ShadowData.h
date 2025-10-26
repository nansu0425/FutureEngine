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