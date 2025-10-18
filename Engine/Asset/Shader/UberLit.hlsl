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
    float3 Padding;
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
#define HAS_DIFFUSE_MAP  (1 << 0)
#define HAS_AMBIENT_MAP  (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_NORMAL_MAP   (1 << 3)
#define HAS_ALPHA_MAP    (1 << 4)
#define HAS_BUMP_MAP     (1 << 5)

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

float4 CalculateDirectionalLight(FDirectionalLightInfo info, float3 worldNormal, float3 worldPos, float3 viewPos)
{
    float3 lightDir = normalize(-info.Direction);
    float NdotL = saturate(dot(worldNormal, lightDir));
    
    float4 diffuse = info.Color * info.Intensity * NdotL;
    
#if LIGHTING_MODEL_PHONG
    // Specular calculation for Phong
    float3 viewDir = normalize(viewPos - worldPos);
    float3 reflectDir = reflect(-lightDir, worldNormal);
    float specular = pow(saturate(dot(viewDir, reflectDir)), Ns);
    diffuse += Ks * specular * info.Intensity;
#endif
    
    return diffuse;
}

float4 CalculatePointLight(FPointLightInfo info, float3 worldNormal, float3 worldPos, float3 viewPos)
{
    float3 lightDir = info.Position - worldPos;
    float distance = length(lightDir);
    
    if (distance > info.Range)
        return float4(0, 0, 0, 0);
    
    lightDir = normalize(lightDir);
    float NdotL = saturate(dot(worldNormal, lightDir));
    
    float attenuation = 1.0f - saturate(distance / info.Range);
    attenuation *= attenuation;
    
    float4 diffuse = info.Color * info.Intensity * NdotL * attenuation;
    
#if LIGHTING_MODEL_PHONG
    // Specular calculation for Phong
    float3 viewDir = normalize(viewPos - worldPos);
    float3 reflectDir = reflect(-lightDir, worldNormal);
    float specular = pow(saturate(dot(viewDir, reflectDir)), Ns);
    diffuse += Ks * specular * info.Intensity * attenuation;
#endif
    
    return diffuse;
}

float4 CalculateSpotLight(FSpotLightInfo info, float3 worldNormal, float3 worldPos, float3 viewPos)
{
    float3 lightDir = info.Position - worldPos;
    float distance = length(lightDir);
    
    if (distance > info.Range)
        return float4(0, 0, 0, 0);
    
    lightDir = normalize(lightDir);
    float3 spotDir = normalize(info.Direction);
    
    float spotAngle = dot(-lightDir, spotDir);
    float spotCutoff = cos(info.SpotAngle);
    
    if (spotAngle < spotCutoff)
        return float4(0, 0, 0, 0);
    
    float NdotL = saturate(dot(worldNormal, lightDir));
    
    float attenuation = 1.0f - saturate(distance / info.Range);
    attenuation *= attenuation;
    
    float spotFactor = pow(spotAngle, 2.0f);
    
    float4 diffuse = info.Color * info.Intensity * NdotL * attenuation * spotFactor;
    
#if LIGHTING_MODEL_PHONG
    // Specular calculation for Phong
    float3 viewDir = normalize(viewPos - worldPos);
    float3 reflectDir = reflect(-lightDir, worldNormal);
    float specular = pow(saturate(dot(viewDir, reflectDir)), Ns);
    diffuse += Ks * specular * info.Intensity * attenuation * spotFactor;
#endif
    
    return diffuse;
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
    // Calculate lighting in vertex shader

    // ambient 계산
    Output.LightColor = CalculateAmbientLight(Ambient) * Ka;
    Output.LightColor += CalculateDirectionalLight(Directional, Output.WorldNormal, Output.WorldPosition, ViewWorldLocation) * Kd;

    [unroll]
    for (int i = 0; i < NumPointLights  && i < NUM_POINT_LIGHT; i++)
    {
        Output.LightColor += CalculatePointLight(PointLights[i], Output.WorldNormal, Output.WorldPosition, ViewWorldLocation) * Kd;
    }

    [unroll]
    for (int j = 0; j < NumSpotLights && i < NUM_SPOT_LIGHT; j++)
    {
        Output.LightColor += CalculateSpotLight(SpotLights[j], Output.WorldNormal, Output.WorldPosition, ViewWorldLocation) * Kd;
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
    // Use pre-calculated vertex lighting
    finalPixel.rgb = Input.LightColor.rgb * diffuseColor.rgb;
    
#elif LIGHTING_MODEL_LAMBERT || LIGHTING_MODEL_PHONG
    // Calculate lighting in pixel shader
    float4 lighting = CalculateAmbientLight(Ambient) * ambientColor;
    //float4 lighting = CalculateAmbientLight(Ambient);
    lighting += CalculateDirectionalLight(Directional, normalize(Input.WorldNormal), Input.WorldPosition, ViewWorldLocation) * diffuseColor;
    
    [unroll]
    for (int i = 0; i < NumPointLights  && i < NUM_POINT_LIGHT; i++)
    {
        lighting += CalculatePointLight(PointLights[i], normalize(Input.WorldNormal), Input.WorldPosition, ViewWorldLocation) * diffuseColor;
    }
    
    [unroll]
    for (int j = 0; j < NumSpotLights && i < NUM_SPOT_LIGHT; j++)
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
