cbuffer constants : register(b0)
{
	row_major float4x4 World;
	row_major float4x4 WorldInverseTranspose;
}

cbuffer PerFrame : register(b1)
{
	row_major float4x4 View;		// View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection;	// Projection Matrix Calculation of MVP Matrix
};

cbuffer DecalConstants : register(b2)
{
	row_major float4x4 DecalWorld;
	row_major float4x4 DecalWorldInverse;
};

Texture2D DecalTexture : register(t0);
SamplerState DecalSampler : register(s0);

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
	float4 WorldPos : TEXCOORD0;
	float4 Normal : TEXCOORD1;
	float2 Tex : TEXCOORD2;
};

PS_INPUT mainVS(VS_INPUT Input)
{
	PS_INPUT Output;

	float4 Pos = mul(float4(Input.Position, 1.0f), World);
	Output.Position = mul(mul(Pos, View), Projection);
	Output.WorldPos = Pos;
	Output.Normal = normalize(mul(float4(Input.Normal, 0.0f), WorldInverseTranspose));
	Output.Tex = Input.Tex;

	return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
	// Normal Test
	float4 DecalForward = mul(float4(1.0f, 0.0f, 0.0f, 0.0f), DecalWorld);
	if (dot(DecalForward, Input.Normal) > 0.0f) {
		//discard;
	}
	
	// Decal Local Transition
	float3 DecalLocalPos = mul(Input.WorldPos, DecalWorldInverse).xyz;
	if (abs(DecalLocalPos.x) > 0.5f || abs(DecalLocalPos.y) > 0.5f || abs(DecalLocalPos.z) > 0.5f) { discard; }

	// UV Transition ([-0.5~0.5], [-0.5~0.5]) -> ([0~1.0], [1.0~0])
	float2 DecalUV = DecalLocalPos.yz * float2(1, -1) + 0.5f;
	float4 DecalColor = DecalTexture.Sample(DecalSampler, DecalUV);
	if (DecalColor.a < 0.001f) { discard; }
	
	return DecalColor;
}