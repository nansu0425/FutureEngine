/**
 * @file BoxTextureFilter.hlsl
 * @brief 분리 가능한 박스 필터 - Row/Column 통합 셰이더
 *
 * @author geb0598 
 * @date 2025-10-27
 */

// --- 고정 매개변수 (커널 관련) ---

// 1. 박스 커널 반지름 (Radius)
// 0 ~ KERNEL_RADIUS까지 총 (KERNEL_RADIUS * 2 + 1)개의 픽셀을 샘플링.
static const int KERNEL_RADIUS = 7;

// (박스 필터는 모든 가중치가 1.0이므로 WEIGHTS 배열이 필요 없음)

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
    uint RegionStartX;
    uint RegionStartY;
    uint RegionWidth;  
    uint RegionHeight;
    uint TextureWidth;
    uint TextureHeight;
}

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

    // KernelRadius에 따라 -R 부터 +R 까지 샘플링
    for (int i = -KERNEL_RADIUS; i <= KERNEL_RADIUS; ++i)
    {
        uint2 SampleCoord; // 샘플링할 픽셀 좌표
        int CurrentOffset = i;

#ifdef SCAN_DIRECTION_COLUMN
        // --- Vertical Pass 모드 ---
        // Y(행) 방향으로 샘플링
        
        int SampleRow = clamp((int)PixelCoord.y + CurrentOffset, RegionStartY, RegionStartY + RegionHeight - 1);
        
        SampleCoord = uint2(PixelCoord.x, (uint)SampleRow);

#else
        // --- Horizontal Pass 모드 (Default) ---
        // X(열) 방향으로 샘플링
        
        int SampleCol = clamp((int)PixelCoord.x + CurrentOffset, RegionStartX, RegionStartX + RegionWidth - 1);

        SampleCoord = uint2((uint)SampleCol, PixelCoord.y);
        
#endif

        // --- 3. 값 누적 ---
        
        // 박스 필터는 가중치가 1.0이므로 바로 더함
        AccumulatedValue += InputTexture[SampleCoord];
    }

    // --- 4. 데이터 저장 (Global Texture) ---
    
    // 총 샘플 수 (커널의 총 너비)
    const float TotalSamples = (float)(KERNEL_RADIUS * 2 + 1);
    
    // 정규화 (총 샘플 수로 나누어 평균 계산)
    OutputTexture[PixelCoord] = AccumulatedValue / TotalSamples;
}