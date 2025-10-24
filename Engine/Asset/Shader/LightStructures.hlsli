#ifndef LIGHT_STRUCTURES_HLSLI
#define LIGHT_STRUCTURES_HLSLI

struct FAmbientLightInfo
{
    float4 Color;
    float Intensity;
    float3 Padding;
};

struct FDirectionalLightInfo
{
    float4 Color;
    float3 Direction;
    float Intensity;

    // Shadow parameters
    row_major float4x4 LightViewProjection;
    uint CastShadow;           // 0 or 1
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
};

struct FPointLightInfo
{
    float4 Color;
    float3 Position;
    float Intensity;
    float Range;
    float DistanceFalloffExponent;

    // Shadow parameters
    uint CastShadow;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    float2 Padding;  // 8 bytes padding for 16-byte alignment (C++ FVector4 alignment)
};

struct FSpotLightInfo
{
    float4 Color;
    float3 Position;
    float Intensity;
    float Range;
    float DistanceFalloffExponent;
    float InnerConeAngle;
    float OuterConeAngle;
    float AngleFalloffExponent;
    float3 Direction;

    // Shadow parameters
    row_major float4x4 LightViewProjection;
    uint CastShadow;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
};

#endif
