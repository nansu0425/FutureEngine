/**
 * @file SummedAreaTextureFilter.hlsl
 * @brief Summed Area 계산용 Hillis-Steele 병렬 누적 합 (Inclusive Scan) - Row/Column 통합 셰이더
 *
 * @details
 * C++(Host)에서 셰이더 컴파일 시 매크로 정의 여부에 따라 동작이 변경된다.
 *
 * 1. ROW SCAN (매크로 미정의 시 - Default)
 * - 입력 텍스처의 각 '행(Row)'을 기준으로 누적 합을 계산한다.
 * - C++ Dispatch: Dispatch(1, TextureHeight, 1)
 * - 스레드 그룹: [numthreads(THREAD_BLOCK_SIZE, 1, 1)] (1 그룹 = 1 행)
 * - GroupID.y = RowIndex, GroupIndex = ColumnIndex
 * - 가정: TextureWidth <= THREAD_BLOCK_SIZE
 *
 * 2. COLUMN SCAN (SCAN_DIRECTION_COLUMN 매크로 정의 시)
 * - 입력 텍스처의 각 '열(Column)'을 기준으로 누적 합을 계산한다.
 * - C++ Dispatch: Dispatch(TextureWidth, 1, 1)
 * - 스레드 그룹: [numthreads(THREAD_BLOCK_SIZE, 1, 1)] (1 그룹 = 1 열)
 * - GroupID.x = ColumnIndex, GroupIndex = RowIndex
 * - 가정: TextureHeight <= THREAD_BLOCK_SIZE
 *
 * <주의사항>
 * 이 셰이더는 스캔하는 방향의 텍스처 크기가 
 * THREAD_BLOCK_SIZE (예: 1024)보다 작거나 같다고 가정한다.
 * 
 * @author geb0598
 * @date 2025-10-26
 */

// 스레드 그룹 당 스레드 개수 (1D)
#define THREAD_BLOCK_SIZE 1024

Texture2D<float2> InputTexture : register(t0);

RWTexture2D<float2> OutputTexture : register(u0);

groupshared float2 SharedMemory[THREAD_BLOCK_SIZE];

cbuffer TextureInfo : register(b0)
{
    uint TextureWidth;
    uint TextureHeight;
}

[numthreads(THREAD_BLOCK_SIZE, 1, 1)]
void mainCS(
    uint3 GroupID : SV_GroupID,
    uint3 GroupThreadID : SV_GroupThreadID,
    uint GroupIndex : SV_GroupIndex
    )
{
    // --- 1. 인덱스 계산 ---
    
    // 그룹 내 현재 스레드의 1D 인덱스 (0 ~ 1023)
    uint ThreadIndex = GroupIndex;
    
    uint Row;
    uint Column;
    uint MaxElementsInThisPass; // 현재 패스에서 스캔할 방향의 최대 요소 개수

#ifdef SCAN_DIRECTION_COLUMN
    // --- Column Scan 모드 ---
    // 1개 스레드 그룹이 1개의 '열'을 처리
    // C++ Dispatch: Dispatch(TextureWidth, 1, 1)
    
    Row = ThreadIndex;          // 스레드 인덱스(0~1023)가 '행' 인덱스가 됩니다.
    Column = GroupID.x;         // 그룹 ID(X)가 '열' 인덱스가 됩니다.
    MaxElementsInThisPass = TextureHeight; // 경계 검사는 '높이' 기준

#else
    // --- Row Scan 모드 (Default) ---
    // 1개 스레드 그룹이 1개의 '행'을 처리
    // C++ Dispatch: Dispatch(1, TextureHeight, 1)

    Row = GroupID.y;            // 그룹 ID(Y)가 '행' 인덱스
    Column = ThreadIndex;       // 스레드 인덱스(0~1023)가 '열' 인덱스
    MaxElementsInThisPass = TextureWidth;  // 경계 검사는 '너비'를 기준
    
#endif

    // --- 2. 데이터 로드 (Global Texture -> Shared Memory) ---
    
    float2 CurrentValue = float2(0.0f, 0.0f);
    
    if (ThreadIndex < MaxElementsInThisPass)
    {
        CurrentValue = InputTexture[uint2(Column, Row)];
    }
    
    SharedMemory[ThreadIndex] = CurrentValue;

    GroupMemoryBarrierWithGroupSync();

    // --- 3. Hillis-Steele 누적 합 (Shared Memory 내에서 수행) ---
    
    for (uint Stride = 1; Stride < THREAD_BLOCK_SIZE; Stride <<= 1)
    {
        float2 NeighborValue = float2(0.0f, 0.0f);
        if (ThreadIndex >= Stride)
        {
            NeighborValue = SharedMemory[ThreadIndex - Stride];
        }

        // 중요: 모든 스레드가 'NeighborValue'를 SharedMemory에서 읽을 때까지 대기
        GroupMemoryBarrierWithGroupSync();

        SharedMemory[ThreadIndex] += NeighborValue;
        
        // 중요: 모든 스레드가 갱신된 값을 'SharedMemory'에 쓸 때까지 대기
        GroupMemoryBarrierWithGroupSync();
    }

    // --- 4. 데이터 저장 (Shared Memory -> Global Texture) ---
    
    if (ThreadIndex < MaxElementsInThisPass)
    {
        OutputTexture[uint2(Column, Row)] = SharedMemory[ThreadIndex];
    }
}
