cbuffer FogConstant : register(b0)
{
	float4 FogColor;
	float FogDensity;
	float FogHeightFalloff;
	float StartDistance;
	float FogCutoffDistance;
	float FogMaxOpacity;
}

cbuffer CameraInverse : register(b1)
{
	row_major float4x4 ViewInverse;
	row_major float4x4 ProjectionInverse;
	float2 ViewportSize;
};

Texture2D DepthTexture : register(t0);
SamplerState DepthSampler : register(s0);

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float4 Color : COLOR;
	float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
};

PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;
	Output.Position = float4(Input.Position.xyz, 1.f);
	return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	float2 UV = Input.Position.xy / ViewportSize;
}