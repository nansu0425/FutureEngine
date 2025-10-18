// UberLit.hlsl - Uber Shader with Multiple Lighting Models
// Supports: Gouraud, Lambert, Phong lighting models

#define NUM_POINT_LIGHT 16
#define NUM_SPOT_LIGHT 16

// Light Structure Definitions
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
};

struct FPointLightInfo
{
    float4 Color;
    float3 Position;
    float Range;
    float Intensity;
    float DistanceFalloffExponent;
    float2 Padding;
};

struct FSpotLightInfo
{
    float4 Color;
    float3 Position;
    float Range;
    float3 Direction;
    float SpotAngle;
    float Intensity;
    float3 Padding;
};

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

cbuffer MaterialConstants : register(b2)
{
    float4 Ka; // Ambient color
    float4 Kd; // Diffuse color
    float4 Ks; // Specular color
    float Ns;  // Specular exponent
    float Ni;  // Index of refraction
    float D;   // Dissolve factor
    uint MaterialFlags; // Which textures are available (bitfield)
    float Time;
}

cbuffer Lighting : register(b3)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional;
    FPointLightInfo PointLights[NUM_POINT_LIGHT];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHT];
    uint NumPointLights;
    uint NumSpotLights;
    float2 PaddingLighting;
}

// Textures
Texture2D DiffuseTexture : register(t0);
Texture2D AmbientTexture : register(t1);
Texture2D SpecularTexture : register(t2);
Texture2D NormalTexture : register(t3);
Texture2D AlphaTexture : register(t4);
Texture2D BumpTexture : register(t5);

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP  (1 << 0) // map_Kd
#define HAS_AMBIENT_MAP  (1 << 1) // map_Ka
#define HAS_SPECULAR_MAP (1 << 2) // map_Ks
#define HAS_NORMAL_MAP   (1 << 3) // map_Ns
#define HAS_ALPHA_MAP    (1 << 4) // map_d
#define HAS_BUMP_MAP     (1 << 5) // map_bump

// Vertex Shader Input/Output
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
    float3 WorldPosition : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 Tex : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float4 LightColor : COLOR0;
#endif
};

struct PS_OUTPUT
{
    float4 SceneColor : SV_Target0;
    float4 NormalData : SV_Target1;
};

// Lighting Calculation Functions
float4 CalculateAmbientLight(FAmbientLightInfo info)
{
    return info.Color * info.Intensity;
}

float4 CalculateDirectionalLight(FDirectionalLightInfo Info, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    float3 LightDir = normalize(-Info.Direction);
    float NdotL = saturate(dot(WorldNormal, LightDir));
    
    float4 diffuse = Info.Color * Info.Intensity * NdotL;
    
#if LIGHTING_MODEL_BlinnPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong)
    float3 WorldToCameraVector = normalize(ViewPos - WorldPos);
    float3 WorldToLightVector = LightDir;
    
    float3 H = normalize(WorldToLightVector + WorldToCameraVector);
    float Spec = pow(saturate(dot(WorldNormal, H)), 64.0f);
    float3 SpecularColor = float3(1.0f, 0.9f, 0.8f);
    float3 Specular = SpecularColor * Spec  * 0.5f;
    float4 SpecularLight = float4(Specular, 0.0f);
    return diffuse + Ks * SpecularLight * Info.Intensity;
    
#endif
    
    return diffuse;
}

float4 CalculatePointLight(FPointLightInfo Info, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    float3 LightDir = Info.Position - WorldPos;
    float Distance = length(LightDir);
    
    if (Distance > Info.Range)
        return float4(0, 0, 0, 0);
    
    LightDir = normalize(LightDir);
    float NdotL = saturate(dot(WorldNormal, LightDir));
    
    float R = Distance / Info.Range;
    float Attenuation = saturate(1.0f - pow(R, Info.DistanceFalloffExponent));
    
    float4 Diffuse = Info.Color * Info.Intensity * NdotL * Attenuation;
    
#if LIGHTING_MODEL_BlinnPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong)
    float3 WorldToCameraVector = normalize(ViewPos - WorldPos);
    float3 WorldToLightVector = normalize(Info.Position - WorldPos);
    
    float3 H = normalize(WorldToLightVector + WorldToCameraVector);
    float Spec = pow(saturate(dot(WorldNormal, H)), 64.0f);
    float3 SpecularColor = float3(1.0f, 0.9f, 0.8f);
    float3 Specular = SpecularColor * Spec  * 0.5f;
    float4 SpecularLight = float4(Specular, 0.0f);
    return Diffuse + Ks * SpecularLight * Info.Intensity;
    
#endif
    
    return Diffuse;
}

float4 CalculateSpotLight(FSpotLightInfo Info, float3 WorldNormal, float3 WorldPos, float3 ViewPos)
{
    float3 LightDir = Info.Position - WorldPos;
    float Distance = length(LightDir);
    
    if (Distance > Info.Range)
        return float4(0, 0, 0, 0);
    
    LightDir = normalize(LightDir);
    float3 SpotDir = normalize(Info.Direction);
    
    float SpotAngle = dot(-LightDir, SpotDir);
    float SpotCutoff = cos(Info.SpotAngle);
    
    if (SpotAngle < SpotCutoff)
        return float4(0, 0, 0, 0);
    
    float NdotL = saturate(dot(WorldNormal, LightDir));
    
    float Attenuation = 1.0f - saturate(Distance / Info.Range);
    Attenuation *= Attenuation;
    
    float SpotFactor = pow(SpotAngle, 2.0f);
    
    float4 Diffuse = Info.Color * Info.Intensity * NdotL * Attenuation * SpotFactor;
    
#if LIGHTING_MODEL_BlinnPHONG || LIGHTING_MODEL_GOURAUD
    // Specular (Blinn-Phong)
    float3 WorldToCameraVector = normalize(ViewPos - WorldPos);
    float3 WorldToLightVector = normalize(Info.Position - WorldPos);
    
    float3 H = normalize(WorldToLightVector + WorldToCameraVector);
    float Spec = pow(saturate(dot(WorldNormal, H)), 64.0f);
    float3 SpecularColor = float3(1.0f, 0.9f, 0.8f);
    float3 Specular = SpecularColor * Spec  * 0.5f;
    float4 SpecularLight = float4(Specular, 0.0f);
    return Diffuse + Ks * SpecularLight * Info.Intensity * Attenuation * SpotFactor;
    
#endif
    
    return Diffuse;
}

// Vertex Shader
PS_INPUT Uber_VS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    Output.WorldPosition = mul(float4(Input.Position, 1.0f), World).xyz;
    Output.Position = mul(mul(mul(float4(Input.Position, 1.0f), World), View), Projection);
    Output.WorldNormal = normalize(mul(Input.Normal, (float3x3)World));
    Output.Tex = Input.Tex;
    
#if LIGHTING_MODEL_GOURAUD
    // Calculate lighting in vertex shader (Gouraud)
    // Accumulate light only; material and textures are applied in pixel stage
    Output.LightColor = CalculateAmbientLight(Ambient);
    Output.LightColor += CalculateDirectionalLight(Directional, Output.WorldNormal, Output.WorldPosition,
    ViewWorldLocation);

    [unroll]
    for (int i = 0; i < NumPointLights && i < NUM_POINT_LIGHT; i++)
    {
        Output.LightColor += CalculatePointLight(PointLights[i], Output.WorldNormal, Output.WorldPosition, ViewWorldLocation);
    }

    [unroll]
    for (int j = 0; j < NumSpotLights && j < NUM_SPOT_LIGHT; j++)
    {
        Output.LightColor += CalculateSpotLight(SpotLights[j], Output.WorldNormal, Output.WorldPosition, ViewWorldLocation);
    }
#endif
    
    return Output;
}

// Pixel Shader
PS_OUTPUT Uber_PS(PS_INPUT Input) : SV_TARGET
{
    PS_OUTPUT Output;
    
    float4 finalPixel = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float2 UV = Input.Tex;
    
    // Sample textures
    float4 diffuseColor = Kd;
    if (MaterialFlags & HAS_DIFFUSE_MAP)
    {
        diffuseColor *= DiffuseTexture.Sample(SamplerWrap, UV);
        finalPixel.a = diffuseColor.a;
    }
    
    float4 ambientColor = Ka;
    if (MaterialFlags & HAS_AMBIENT_MAP)
    {
        ambientColor *= AmbientTexture.Sample(SamplerWrap, UV);
    }
    
#if LIGHTING_MODEL_GOURAUD
    // Use pre-calculated vertex lighting; apply diffuse material/texture per-pixel
    finalPixel.rgb = Input.LightColor.rgb * diffuseColor.rgb;
    
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_BLINNPHONG
    // Calculate lighting in pixel shader
    float4 lighting = CalculateAmbientLight(Ambient) * ambientColor;
    lighting += CalculateDirectionalLight(Directional, normalize(Input.WorldNormal), Input.WorldPosition, ViewWorldLocation) * diffuseColor;
    
    [unroll]
    for (int i = 0; i < NumPointLights  && i < NUM_POINT_LIGHT; i++)
    {
        lighting += CalculatePointLight(PointLights[i], normalize(Input.WorldNormal), Input.WorldPosition, ViewWorldLocation) * diffuseColor;
    }
    
    [unroll]
    for (int j = 0; j < NumSpotLights && j < NUM_SPOT_LIGHT; j++)
    {
        lighting += CalculateSpotLight(SpotLights[j], normalize(Input.WorldNormal), Input.WorldPosition, ViewWorldLocation) * diffuseColor;
    }
    
    finalPixel.rgb = lighting.rgb;
    
#else
    // Fallback: simple textured rendering (like current TexturePS.hlsl)
    finalPixel.rgb = diffuseColor.rgb + ambientColor.rgb;
#endif
    
    // Alpha handling
    if (MaterialFlags & HAS_ALPHA_MAP)
    {
        float alpha = AlphaTexture.Sample(SamplerWrap, UV).r;
        finalPixel.a = D * alpha;
    }
    
    Output.SceneColor = finalPixel;
    
    // Encode normal for deferred rendering
    float3 encodedNormal = normalize(Input.WorldNormal) * 0.5f + 0.5f;
    Output.NormalData = float4(encodedNormal, 1.0f);
    
    return Output;
}
