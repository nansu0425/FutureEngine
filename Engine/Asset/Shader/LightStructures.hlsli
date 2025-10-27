#ifndef LIGHT_STRUCTURES_HLSLI
#define LIGHT_STRUCTURES_HLSLI

/*-----------------------------------------------------------------------------
    Enums
 -----------------------------------------------------------------------------*/

// --- enum class EShadowModeIndex ---

static const uint SMI_UnFiltered     = 0;
static const uint SMI_PCF            = 1;
static const uint SMI_VSM            = 2;
static const uint SMI_VSM_BOX        = 3;
static const uint SMI_VSM_GAUSSIAN   = 4;
static const uint SMI_SAVSM          = 5;

/*-----------------------------------------------------------------------------
    Structs
 -----------------------------------------------------------------------------*/

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
    uint ShadowModeIndex;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    float3 Padding;
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
    uint ShadowModeIndex;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    float Padding;  // 4 bytes padding for 16-byte alignment (C++ FVector4 alignment)
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
    uint ShadowModeIndex;
    float ShadowBias;
    float ShadowSlopeBias;
    float ShadowSharpen;
    float3 Padding;
};

#endif
