// ================================================================
// PointLightShader.hlsl
// - Deferred/Hybrid Post Lighting Pass
// - Input : SceneColor, NormalData, Depth
// - Output : LitScene (SceneColor + Point Light contribution)
// ================================================================

// ------------------------------------------------
// Constant Buffers
// ------------------------------------------------
cbuffer PerFrame : register(b0)
{
    row_major float4x4 InvView;        // Inverse of View matrix
    row_major float4x4 InvProjection;  // Inverse of Projection matrix
    float3 CameraPosition;             // For specular calculation
    float Padding0;
};

cbuffer PointLightData : register(b1)
{
    float3 LightPosition;  // World-space position of light
    float  LightIntensity; // Strength (brightness)
    float3 LightColor;     // RGB color
    float  LightRadius;    // Max effective range
    float  LightFalloffExtent; // Falloff sharpness
    float3 Padding1;
};

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D SceneColorTex : register(t0);
Texture2D NormalTex     : register(t1);
Texture2D DepthTex      : register(t2);

SamplerState LinearSampler : register(s0);

// ------------------------------------------------
// Fullscreen Quad Vertex Shader
// ------------------------------------------------
struct VS_INPUT
{
    float3 position : POSITION; // e.g. fullscreen quad position (-1~1)
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 positionCS : SV_POSITION;
    float2 tex        : TEXCOORD0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.positionCS = float4(input.position, 1.0f);
    output.tex = input.tex;
    return output;
}

// ------------------------------------------------
// Helper : Reconstruct view-space position from depth
// ------------------------------------------------
float3 ReconstructWorldPosition(float2 uv, float depth)
{
    // depth : [0,1] from DepthTex
    float4 clipPos = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, InvProjection);
    viewPos /= viewPos.w;

    // To world-space
    float4 worldPos = mul(viewPos, InvView);
    return worldPos.xyz;
}

// ------------------------------------------------
// Pixel Shader
// ------------------------------------------------
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float2 uv = input.tex;

    // Read base data
    float4 baseColor = SceneColorTex.Sample(LinearSampler, uv);
    float4 encodedNormal = NormalTex.Sample(LinearSampler, uv);
    float depth = DepthTex.Sample(LinearSampler, uv).r;

    // Invalid depth check
    if (depth >= 1.0f)
        return baseColor;

    // Decode normal
    float3 normal = normalize(encodedNormal.xyz * 2.0f - 1.0f);

    // Reconstruct world position
    float3 worldPos = ReconstructWorldPosition(uv, depth);

    // Compute lighting vector
    float3 L = LightPosition - worldPos;
    float dist = length(L);
    L /= dist;

    // Light attenuation
    float attenuation = saturate(1.0f - pow(dist / LightRadius, LightFalloffExtent));
    attenuation *= LightIntensity;

    // Diffuse term
    float NdotL = saturate(dot(normal, L));
    float3 diffuse = LightColor * NdotL * attenuation;

    // Specular (Blinn-Phong)
    float3 V = normalize(CameraPosition - worldPos);
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(normal, H)), 32.0f); // fixed shininess
    float3 specular = LightColor * spec * attenuation * 0.5f;

    // Combine
    float3 finalLighting = diffuse + specular;

    // Additive blending
    float3 result = baseColor.rgb + finalLighting;

    return float4(result, baseColor.a);
}