#include "TextureVS.hlsl"

float4 mainPS(PS_INPUT Input) : SV_Target
{
    float Depth = length(Input.WorldPosition - ViewWorldLocation);
    float Range = saturate((Depth - NearClip) / (FarClip - NearClip));
    return float4(Range, Range, Range, 1.f);
}
