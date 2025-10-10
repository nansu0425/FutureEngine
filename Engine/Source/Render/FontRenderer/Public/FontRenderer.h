#pragma once

class URenderer;

class UFontRenderer
{
public:
    struct FFontVertex
    {
        FVector Position;        // 3D 월드 좌표
        FVector2 TexCoord;      // 쿼드 내 UV 좌표 (0~1)
        uint32 CharIndex;       // ASCII 문자 코드
    };

    struct FFontConstantBuffer
    {
        FVector2 AtlasSize = FVector2(512.0f, 512.0f);      // 아틀라스 전체 크기
        FVector2 GlyphSize = FVector2(16.0f, 16.0f);         // 한 글자 크기
        FVector2 GridSize = FVector2(32.0f, 32.0f);          // 아틀라스 그리드 (32x32 글자)
        FVector2 Padding = FVector2(0.0f, 0.0f);             // 여백
    };

    UFontRenderer();
    ~UFontRenderer();

    bool Initialize();
    void Release();
};