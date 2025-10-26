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

struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    // 1. World space로 변환
    float4 WorldPos = mul(float4(Input.Position, 1.0f), World);

    // 2. View-Projection으로 한 번에 clip space로 변환
    Output.Position = mul(WorldPos, ViewProjection);

    return Output;
}

float2 mainPS(PS_INPUT Input) : SV_Target0
{
    float Depth = Input.Position.z;
    
    /*-----------------------------------------------------------------------------
        1. 1차 모멘트 (First Moment): E(X)
     -----------------------------------------------------------------------------*/
    /*
     * 확률 변수 X를 깊이(Depth)라 할 때, 
     * E(X)는 해당 픽셀의 평균 깊이를 의미함.
     * 그림자 필터링 시 '기대값' 역할을 하며, 
     *깊이의 평균값(첫 번째 통계적 모멘트)에 해당.
     */
    float M1 = Depth; 

    /*-----------------------------------------------------------------------------
        2. 2차 모멘트 (Second Moment): E(X²)
     -----------------------------------------------------------------------------*/
    /*
     * 통계적으로, 분산(Variance)은 Var(X) = E(X²) - [E(X)]² 로 표현됨.
     * 따라서 E(X²) = Depth² + Var(X)
     * 
     * 여기서는 픽셀 단위에서의 깊이 변화량(ddx, ddy)을 사용하여
     * 깊이의 분산(Variance)을 근사적으로 추정함.
     *
     * f(x, y)를 Depth 함수로 두면,
     * ∂f/∂x, ∂f/∂y는 픽셀 간의 깊이 기울기(gradient)에 해당함.
     */
    float dx = ddx(Depth);
    float dy = ddy(Depth);
    
    /*-----------------------------------------------------------------------------
        3. Analytic Variance Bias 계산
     -----------------------------------------------------------------------------*/
    /*
     * 깊이의 공간적 변화율을 이용함. 
     * 픽셀 내의 깊이 분포를 대략적으로 픽셀 절반 만큼의 표준편차를 가지는 가우시안(Gaussian) 형태라 가정함.
     */ 
    float AnalyticVarianceBias = 0.25f * (dx * dx + dy * dy);
    
    float M2 = Depth * Depth + AnalyticVarianceBias;
    
    return float2(M1, M2);
}