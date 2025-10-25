// Point Light Shadow Depth Shader
//
// Point light shadow는 linear distance를 depth로 저장합니다.
// Perspective depth 대신 (distance / range)를 사용하여 cube map의 모든 면에서 일관된 비교가 가능합니다.

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
