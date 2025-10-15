// ================================================================
// SceneDepthView.hlsl
// - Renders a linearized depth view for the editor.
// - Input: Depth Texture
// - Output: Grayscale Linear Depth
// ================================================================

// ------------------------------------------------
// Constant Buffers
// ------------------------------------------------
cbuffer PerFrameConstants : register(b0)
{
    float2 RenderTargetSize;                   // Viewport rectangle (x, y, width, height)
};

cbuffer Camera : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float3 ViewWorldLocation;    
    float NearClip;
    float FarClip;
};

// ------------------------------------------------
// Textures and Sampler
// ------------------------------------------------
Texture2D DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

// ------------------------------------------------
// Vertex and Pixel Shader I/O
// ------------------------------------------------
struct PS_INPUT
{
    float4 Position : SV_POSITION;
};

// ================================================================
// Vertex Shader
// - Generates a full-screen triangle without needing a vertex buffer.
// ================================================================
PS_INPUT mainVS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;

    // SV_VertexID를 사용하여 화면을 덮는 큰 삼각형의 클립 공간 좌표를 생성
    // ID 0 -> (-1, 1), ID 1 -> (3, 1), ID 2 -> (-1, -3) -- 수정된 좌표계
    // 이 좌표계는 UV가 (0,0)부터 시작하도록 조정합니다.
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    output.Position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    output.Position.y *= -1.0f;

    return output;
}

// ================================================================
// Pixel Shader
// - Samples non-linear depth and converts it to linear depth.
// ================================================================
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    float2 ScreenPosition = Input.Position.xy;
    float2 uv = ScreenPosition / RenderTargetSize;
    
    float nonLinearDepth = DepthTexture.Sample(PointSampler, uv).r;
    float viewSpaceDepth = (FarClip * NearClip) / (nonLinearDepth * (NearClip - FarClip) + FarClip);
    float linearDepth = saturate(viewSpaceDepth / FarClip);

    // 5. [수정] 깊이 값을 밴드 크기로 나누어 스케일링
    //    linearDepth가 0~1 사이를 변할 때, scaledDepth는 0~1/BandSize 사이를 변하게 됨
    float BandSize = 0.02f; // 또는 상수 버퍼에서 이 값을 받아옴
    float scaledDepth = linearDepth / BandSize;

    // 6. [추가] frac 함수로 0~1 사이를 반복하는 톱니파(sawtooth wave) 패턴 생성
    //    frac(3.7)의 결과는 0.7
    float sawtooth = frac(scaledDepth);

    // 7. [추가] "흰색 -> 검은색"으로 변하도록 패턴을 반전
    float bandPattern = 1.0f - sawtooth;

    // 8. 최종 밴드 패턴을 회색조로 출력
    return float4(bandPattern, bandPattern, bandPattern, 1.0f);
}