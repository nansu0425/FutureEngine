// HitProxyShader.hlsl
// Color Picking for Component Selection

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

cbuffer HitProxyColor : register(b2)
{
    float4 ProxyColor;  // RGB = HitProxyId, A = 1.0
}

// Vertex Input
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
};

// Vertex Output / Pixel Input
struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

// Vertex Shader
PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    // Transform to world space
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);

    // Transform to view space
    float4 ViewPos = mul(WorldPos, View);

    // Transform to clip space
    Output.Position = mul(ViewPos, Projection);

    return Output;
}

// Pixel Shader
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    return ProxyColor;
}
