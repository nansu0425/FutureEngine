/**
 * @file GaussianTextureFilter.hlsl
 * @brief 분리 가능한 가우시안 필터 - Row/Column 통합 셰이더
 *
 * @details
 * 가우시안 커널 가중치는 셰이더 내에 상수로 고정되어 있다.
 *
 * @author geb0598
 * @date 2025-10-27
 */

// --- 고정 매개변수 (커널 관련) ---

// 1. 가우시안 커널 반지름 (Radius)
// 0 ~ KERNEL_RADIUS까지 총 (KERNEL_RADIUS * 2 + 1)개의 픽셀을 샘플링.
static const int KERNEL_RADIUS = 7;

// 2. 가우시안 가중치 (미리 계산된 값)
// sigma = 2.5 기준으로 계산된 값 (정규화 불필요)
// Weights[0] = 중앙값, Weights[1] = 1칸 떨어진 값, ...
static const float WEIGHTS[KERNEL_RADIUS + 1] = 
{
    1.000000, // 0
    0.923116, // 1
    0.726149, // 2
    0.486752, // 3
    0.278037, // 4
    0.135335, // 5
    0.056102, // 6
    0.019829  // 7
};

// --- 스레드 그룹 크기 (2D) ---
#define THREAD_BLOCK_SIZE_X 16
#define THREAD_BLOCK_SIZE_Y 16

// 입력 텍스처 (이전 패스의 결과)
Texture2D<float2> InputTexture : register(t0);

// 출력 텍스처 (블러 결과)
RWTexture2D<float2> OutputTexture : register(u0);

// 텍스처 크기 정보
cbuffer TextureInfo : register(b0)
{
    uint TextureWidth;  
    uint TextureHeight; 
}

[numthreads(THREAD_BLOCK_SIZE_X, THREAD_BLOCK_SIZE_Y, 1)]
void mainCS(
    uint3 DispatchThreadID : SV_DispatchThreadID
    )
{
    // --- 1. 인덱스 계산 ---
    
    // 현재 스레드가 처리할 픽셀의 2D 좌표
    uint2 PixelCoord = DispatchThreadID.xy;

    // 텍스처 경계 검사 (cbuffer 값 사용)
    if (PixelCoord.x >= TextureWidth || PixelCoord.y >= TextureHeight)
    {
        return;
    }

    // --- 2. 가우시안 컨볼루션 (가중치 합) ---
    
    float2 AccumulatedValue = float2(0.0f, 0.0f);
    float  TotalWeight = 0.0f;

    // KernelRadius에 따라 -R 부터 +R 까지 샘플링
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        uint2 SampleCoord; // 샘플링할 픽셀 좌표
        int CurrentOffset = i;

#ifdef BLUR_DIRECTION_VERTICAL
        // --- Vertical Pass 모드 ---
        // Y(행) 방향으로 샘플링
        
        // C++의 clamp()와 동일. 경계 밖을 샘플링하지 않도록 방지.
        int SampleRow = (int)PixelCoord.y + CurrentOffset;
        if (SampleRow < 0) SampleRow = 0;
        if (SampleRow >= TextureHeight) SampleRow = TextureHeight - 1;
        
        SampleCoord = uint2(PixelCoord.x, (uint)SampleRow);

#else
        // --- Horizontal Pass 모드 (Default) ---
        // X(열) 방향으로 샘플링
        
        // C++의 clamp()와 동일.
        int SampleCol = (int)PixelCoord.x + CurrentOffset;
        if (SampleCol < 0) SampleCol = 0;
        if (SampleCol >= TextureWidth) SampleCol = TextureWidth - 1;

        SampleCoord = uint2((uint)SampleCol, PixelCoord.y);
        
#endif

        // --- 3. 가중치 합산 ---
        
        // 가중치 (abs(i)를 인덱스로 사용)
        float Weight = WEIGHTS[abs(CurrentOffset)];
        
        // 입력 텍스처에서 값 읽기
        AccumulatedValue += InputTexture[SampleCoord] * Weight;
        TotalWeight += Weight;
    }

    // --- 4. 데이터 저장 (Global Texture) ---
    
    // 정규화 (총 가중치로 나누어 평균 계산)
    OutputTexture[PixelCoord] = AccumulatedValue / TotalWeight;
}