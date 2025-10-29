/**
 * @file BoxTextureFilter.hlsl
 * @brief 분리 가능한 박스 필터 - Row/Column 통합 셰이더
 *
 * @author geb0598 
 * @date 2025-10-27
 */

/*-----------------------------------------------------------------------------
    고정 매개변수 (커널 관련) 
 -----------------------------------------------------------------------------*/

// 0.0 (블러 없음)일 때의 최소 커널 반지름
static const int MIN_KERNEL_SIZE = 1;
// 1.0 (최대 블러)일 때의 최대 커널 반지름
static const int MAX_KERNEL_SIZE = 7;

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

/*-----------------------------------------------------------------------------
    컴퓨트 쉐이더
 -----------------------------------------------------------------------------*/

[numthreads(THREAD_BLOCK_SIZE_X, THREAD_BLOCK_SIZE_Y, 1)]
void mainCS(
    uint3 DispatchThreadID : SV_DispatchThreadID
    )
{
    // 텍스처 경계 검사 (cbuffer 값 사용)
    if (DispatchThreadID.x >= RegionWidth || DispatchThreadID.y >= RegionHeight)
    {
        return;
    }
    
    // --- 1. 인덱스 계산 ---
    
    // 현재 스레드가 처리할 픽셀의 2D 좌표
    uint2 PixelCoord = DispatchThreadID.xy + uint2(RegionStartX, RegionStartY);

    // --- 2. 박스 필터 컨볼루션 (평균 계산) ---
    
    float2 AccumulatedValue = float2(0.0f, 0.0f);

    float ValidSampleCount = 0.0f;

    // KernelRadius에 따라 -R 부터 +R 까지 샘플링
    int KernelRadius = (int)round(lerp((float)MIN_KERNEL_SIZE, (float)MAX_KERNEL_SIZE, FilterStrength));
    for (int i = -KernelRadius; i <= KernelRadius; ++i)
    {
        uint2 SampleCoord; // 샘플링할 픽셀 좌표
        int CurrentOffset = i;

#ifdef SCAN_DIRECTION_COLUMN
        // --- Vertical Pass 모드 ---
        // Y(행) 방향으로 샘플링

        int SampleRow = (int)PixelCoord.y + CurrentOffset;
        if (SampleRow >= (int)RegionStartY && SampleRow < (int)(RegionStartY + RegionHeight))
        {
            SampleCoord = uint2(PixelCoord.x, (uint)SampleRow);

            AccumulatedValue += InputTexture[SampleCoord];
            ValidSampleCount += 1.0f;
        }
#else
        // --- Horizontal Pass 모드 (Default) ---
        // X(열) 방향으로 샘플링

        int SampleCol = (int)PixelCoord.x + CurrentOffset;
        if (SampleCol >= (int)RegionStartX && SampleCol < (int)(RegionStartX + RegionWidth))
        {
            SampleCoord = uint2((uint)SampleCol, PixelCoord.y);
            
            AccumulatedValue += InputTexture[SampleCoord];
            ValidSampleCount += 1.0f;
        }
#endif
    }

    // --- 4. 데이터 저장 (Global Texture) ---

    if (ValidSampleCount > 0.0f)
    {
        OutputTexture[PixelCoord] = AccumulatedValue / ValidSampleCount;
    }
    else
    {
        OutputTexture[PixelCoord] = float2(1.0f, 1.0f);
    }
}