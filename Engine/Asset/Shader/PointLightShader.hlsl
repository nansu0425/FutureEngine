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
    float  Padding0;
    float4 Viewport;                   // Viewport rectangle (x, y, width, height)
    float2 RenderTargetSize;           // Full size of the render target
};

cbuffer PointLightData : register(b1)
{
    float3 LightPosition;      // World-space position of light
    float  LightIntensity;     // Strength (brightness)
    float3 LightColor;         // RGB color
    float  LightRadius;        // Max effective range
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
    float3 position : POSITION;  // fullscreen quad position (-1~1)
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // SV_Position provides screen-space coordinates
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // Fullscreen quad input position is already in clip space (-1~1)
    output.position = float4(input.position, 1.0f);

    return output;
}

// ------------------------------------------------
// Helper : Reconstruct world-space position from depth
// ------------------------------------------------
float3 ReconstructWorldPosition(float2 clipPosXY, float depth)
{
    // depth : [0,1] from DepthTex
    float4 clipPos = float4(clipPosXY, depth, 1.0f);

    // View-space reconstruction
    float4 viewPos = mul(clipPos, InvProjection);
    viewPos /= viewPos.w;

    // World-space conversion
    float4 worldPos = mul(viewPos, InvView);
    return worldPos.xyz;
}

// ------------------------------------------------
// Pixel Shader
// ------------------------------------------------
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // Use absolute screen coordinates from SV_Position
    float2 screenPos = input.position.xy;

    // Calculate UV coordinates based on full render target size
    float2 uv = screenPos / RenderTargetSize;

    // Read G-Buffer data
    float4 baseColor = SceneColorTex.Sample(LinearSampler, uv);
    float4 encodedNormal = NormalTex.Sample(LinearSampler, uv);
    float depth = DepthTex.Sample(LinearSampler, uv).r;

    // Skip background (no depth)
    if (depth >= 1.0f)
        return baseColor;

    // Decode normal
    float3 normal = normalize(encodedNormal.xyz * 2.0f - 1.0f);

    // Reconstruct world position using viewport-aware clip space coordinates
    float2 viewportUV = (screenPos - Viewport.xy) / Viewport.zw;
    float2 viewportClipPos = viewportUV * 2.0 - 1.0;
    viewportClipPos.y *= -1.0; // Flip Y for clip space
    float3 worldPos = ReconstructWorldPosition(viewportClipPos, depth);

    // Lighting vector
    float3 L = LightPosition - worldPos;
    float dist = length(L);
    L /= dist;

    // Attenuation
    float attenuation = saturate(1.0f - pow(dist / LightRadius, LightFalloffExtent));
    attenuation *= LightIntensity;

    // Diffuse
    float NdotL = saturate(dot(normal, L));
    float3 diffuse = LightColor * NdotL * attenuation;

    // Specular (Blinn-Phong)
    float3 V = normalize(CameraPosition - worldPos);
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(normal, H)), 32.0f);
    float3 specular = LightColor * spec * attenuation * 0.5f;

    // Combine lighting
    float3 finalLighting = diffuse + specular;

    // Additive blending
    float3 result = baseColor.rgb + finalLighting;

    return float4(result, 1.0f);
}
