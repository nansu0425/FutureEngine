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
    float2 Moments : SV_Target0;
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

    // 1. 선형 거리 계산 (Light -> Pixel)
    float Distance = length(Input.WorldPosition - LightPosition);

    // 2. [0, 1] 범위로 정규화
    // 이 값이 VSM의 확률 변수 'X' (깊이)가 됩니다.
    float Depth = saturate(Distance / LightRange);

    /*-----------------------------------------------------------------------------
        3. 1차 모멘트 (First Moment): E(X)
     -----------------------------------------------------------------------------*/
    // VSM의 평균값은 정규화된 선형 거리 그 자체입니다.
    float M1 = Depth;

    /*-----------------------------------------------------------------------------
        4. 2차 모멘트 (Second Moment): E(X²)
     -----------------------------------------------------------------------------*/
    // 깊이(선형 거리)의 공간적 변화율(gradient)을 계산합니다.
    float dx = ddx(Depth);
    float dy = ddy(Depth);
    
    // Analytic Variance Bias (분산 편향)
    float AnalyticVarianceBias = 0.25f * (dx * dx + dy * dy);
    
    // M2 = E(X²) ≈ E(X)² + Var(X) ≈ Depth² + AnalyticVarianceBias
    float M2 = Depth * Depth + AnalyticVarianceBias;
    
    /*-----------------------------------------------------------------------------
        5. 출력 값 할당
     -----------------------------------------------------------------------------*/
    
    // RTV (Render Target View)
    // 1차, 2차 모멘트를 저장합니다.
    Output.Moments = float2(M1, M2);
    
    // DSV (Depth Stencil View)
    // 표준 깊이 테스트를 위해 정규화된 선형 거리를 저장합니다.
    Output.Depth = Depth;

    return Output;
}
