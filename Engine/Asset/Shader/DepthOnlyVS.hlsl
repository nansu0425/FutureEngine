// Depth-Only Vertex Shader
//
// 특정 view에서 depth만 렌더링하기 위한 범용 vertex shader입니다.
// Shadow mapping, depth pre-pass 등 다양한 용도로 사용할 수 있습니다.

cbuffer Model : register(b0)
{
    row_major float4x4 World;
}

cbuffer ViewProj : register(b1)
{
    row_major float4x4 ViewProjection;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float4 Tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT mainVS(VS_INPUT Input)
{
    VS_OUTPUT Output;

    // 1. World space로 변환
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);

    // 2. View-Projection으로 한 번에 clip space로 변환
    Output.Position = mul(WorldPos, ViewProjection);

    return Output;
}
