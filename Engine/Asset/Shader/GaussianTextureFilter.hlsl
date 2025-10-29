/**
 * @file GaussianTextureFilter.hlsl
 * @brief 분리 가능한 가우시안 필터 - Row/Column 통합 셰이더
 *
 * @author geb0598
 * @date 2025-10-27
 */

/*-----------------------------------------------------------------------------
    고정 매개변수 (커널 관련) 
 -----------------------------------------------------------------------------*/

// --- 가우시안 커널 반지름 (Radius) ---
// 0 ~ KERNEL_RADIUS까지 총 (KERNEL_RADIUS * 2 + 1)개의 픽셀을 샘플링.
static const int KERNEL_RADIUS = 7;

// --- 표준편차 ---
static const float MIN_SIGMA = 0.1f;
static const float MAX_SIGMA = KERNEL_RADIUS / 2.0f;

// --- 스레드 그룹 크기 (2D) ---
#define THREAD_BLOCK_SIZE_X 16
#define THREAD_BLOCK_SIZE_Y 16

/*-----------------------------------------------------------------------------
    GPU 자원
 -----------------------------------------------------------------------------*/

// 입력 텍스처 (이전 패스의 결과)
Texture2D<float2> InputTexture : register(t0);

// 출력 텍스처 (블러 결과)
RWTexture2D<float2> OutputTexture : register(u0);

// 텍스처 크기 정보
cbuffer TextureInfo : register(b0)
{
    uint RegionStartX;
    uint RegionStartY;
    uint RegionWidth;
    uint RegionHeight;
    uint TextureWidth;  
    uint TextureHeight; 
}

cbuffer FilterInfo : register(b1)
{
    float FilterStrength;
}

groupshared float Weights[KERNEL_RADIUS + 1];

/*-----------------------------------------------------------------------------
    컴퓨트 쉐이더
 -----------------------------------------------------------------------------*/

[numthreads(THREAD_BLOCK_SIZE_X, THREAD_BLOCK_SIZE_Y, 1)]
void mainCS(
    uint GroupIndex        : SV_GroupIndex,
    uint3 DispatchThreadID : SV_DispatchThreadID
    )
{
    // --- 1. 가중치 계산 (그룹당 1회) ---
    if (GroupIndex == 0)
    {
        float Sigma = lerp(MIN_SIGMA, MAX_SIGMA, FilterStrength);

        if (Sigma < 0.001f)
        {
            Weights[0] = 1.0f;
            for (int i = 1; i <= KERNEL_RADIUS; ++i)
            {
                Weights[i] = 0.0f;
            }
        }
        else
        {
            for (int i = 0; i <= KERNEL_RADIUS; ++i)
            {
                float X = (float)i;
                Weights[i] = exp(-(X * X) / (2.0f * Sigma * Sigma));
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();
    
    // 텍스처 경계 검사 (cbuffer 값 사용)
    if (DispatchThreadID.x >= RegionWidth || DispatchThreadID.y >= RegionHeight)
    {
        return;
    }
    
    // --- 2. 인덱스 계산 ---
    
    // 현재 스레드가 처리할 픽셀의 2D 좌표
    uint2 PixelCoord = DispatchThreadID.xy + uint2(RegionStartX, RegionStartY);

    // --- 3. 가우시안 컨볼루션 (가중치 합) ---
    
    float2 AccumulatedValue = float2(0.0f, 0.0f);
    float  TotalWeight = 0.0f;

    // KernelRadius에 따라 -R 부터 +R 까지 샘플링
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        uint2 SampleCoord; // 샘플링할 픽셀 좌표
        int CurrentOffset = i;

        float Weight = Weights[abs(CurrentOffset)];

#ifdef SCAN_DIRECTION_COLUMN
        // --- Vertical Pass 모드 ---
        // Y(행) 방향으로 샘플링
        int SampleRow = (int)PixelCoord.y + CurrentOffset;
        if (SampleRow >= (int)RegionStartY && SampleRow < (int)(RegionStartY + RegionHeight))
        {
            SampleCoord = uint2(PixelCoord.x, (uint)SampleRow);

            AccumulatedValue += InputTexture[SampleCoord] * Weight;
            TotalWeight += Weight;
        }
#else
        // --- Horizontal Pass 모드 (Default) ---
        // X(열) 방향으로 샘플링
        int SampleCol = (int)PixelCoord.x + CurrentOffset;
        if (SampleCol >= (int)RegionStartX && SampleCol < (int)(RegionStartX + RegionWidth))
        {
            SampleCoord = uint2((uint)SampleCol, PixelCoord.y);

            AccumulatedValue += InputTexture[SampleCoord] * Weight;
            TotalWeight += Weight;
        }
#endif
    }

    // --- 4. 데이터 저장 (Global Texture) ---
    if (TotalWeight > 0.0f)
    {
        OutputTexture[PixelCoord] = AccumulatedValue / TotalWeight;
    }
    else
    {
        OutputTexture[PixelCoord] = float2(1.0f, 1.0f); 
    }
}