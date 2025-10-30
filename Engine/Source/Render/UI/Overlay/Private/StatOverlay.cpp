#include "pch.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/UI/Overlay/Public/D2DOverlayManager.h"
#include "Manager/Render/Public/CascadeManager.h"

IMPLEMENT_SINGLETON_CLASS(UStatOverlay, UObject)

UStatOverlay::UStatOverlay() = default;
UStatOverlay::~UStatOverlay() = default;

void UStatOverlay::Initialize()
{
}

void UStatOverlay::Release()
{
}

void UStatOverlay::Render()
{
    TIME_PROFILE(StatDrawn);

    if (IsStatEnabled(EStatType::FPS))
    {
        RenderFPS();
    }
    if (IsStatEnabled(EStatType::Memory))
    {
        RenderMemory();
    }
    if (IsStatEnabled(EStatType::Picking))
    {
        RenderPicking();
    }
    if (IsStatEnabled(EStatType::Time))
    {
        RenderTimeInfo();
    }
    if (IsStatEnabled(EStatType::Decal))
    {
        RenderDecalInfo();
    }
    if (IsStatEnabled(EStatType::Shadow))
    {
        RenderShadowInfo();
    }
}

void UStatOverlay::RenderFPS()
{
    auto& timeManager = UTimeManager::GetInstance();
    CurrentFPS = timeManager.GetFPS();
    FrameTime = timeManager.GetDeltaTime() * 1000;

    char buf[64];
    (void)sprintf_s(buf, sizeof(buf), "FPS: %.1f (%.2f ms)", CurrentFPS, FrameTime);
    FString text = buf;

    float r = 0.5f, g = 1.0f, b = 0.5f;
    if (CurrentFPS < 30.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
    else if (CurrentFPS < 60.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

    RenderText(text, OverlayX, OverlayY, r, g, b);
}

void UStatOverlay::RenderMemory()
{
    float MemoryMB = static_cast<float>(TotalAllocationBytes) / (1024.0f * 1024.0f);

    char Buf[64];
    (void)sprintf_s(Buf, sizeof(Buf), "Memory: %.1f MB (%u objects)", MemoryMB, TotalAllocationCount);
    FString text = Buf;

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    RenderText(text, OverlayX, OverlayY + OffsetY, 1.0f, 1.0f, 0.0f);
}

void UStatOverlay::RenderPicking()
{
    float AvgMs = PickAttempts > 0 ? AccumulatedPickingTimeMs / PickAttempts : 0.0f;

    char Buf[128];
    (void)sprintf_s(Buf, sizeof(Buf), "Picking Time %.2f ms (Attempts %u, Accum %.2f ms, Avg %.2f ms)",
        LastPickingTimeMs, PickAttempts, AccumulatedPickingTimeMs, AvgMs);
    FString Text = Buf;

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;

    float r = 0.0f, g = 1.0f, b = 0.8f;
    if (LastPickingTimeMs > 5.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
    else if (LastPickingTimeMs > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

    RenderText(Text, OverlayX, OverlayY + OffsetY, r, g, b);
}

void UStatOverlay::RenderDecalInfo()
{
    char Buf[128];
    (void)sprintf_s(Buf, sizeof(Buf), "Rendered Decals: %u (Collided: %u)",
        RenderedDecal, CollidedCompCount);
    FString Text = Buf;

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))      OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory))   OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Picking))  OffsetY += 20.0f;

    float r = 0.5f, g = 1.0f, b = 0.5f;
    if (RenderedDecal > 100)
    {
        r = 1.0f; g = 1.0f; b = 0.0f;
    }
    if (RenderedDecal > 500)
    {
        r = 1.0f; g = 0.0f; b = 0.0f;
    }

    RenderText(Text, OverlayX, OverlayY + OffsetY, r, g, b);
}

void UStatOverlay::RenderTimeInfo()
{
    const TArray<FString> ProfileKeys = FScopeCycleCounter::GetTimeProfileKeys();

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Picking)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Decal))  OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Shadow))
    {
        // Shadow Stat: 7 lines base + 3 lines CSM (if directional light exists)
        // DirectionalLightCount가 0보다 크면 추가 3줄
        OffsetY += 140.0f;
        if (DirectionalLightCount > 0)
        {
            OffsetY += 60.0f;
        }
    }

    float CurrentY = OverlayY + OffsetY;
    const float LineHeight = 20.0f;

    for (const FString& Key : ProfileKeys)
    {
        const FTimeProfile& Profile = FScopeCycleCounter::GetTimeProfile(Key);

        char buf[128];
        (void)sprintf_s(buf, sizeof(buf), "%s: %.2f ms", Key.c_str(), Profile.Milliseconds);
        FString text = buf;

        float r = 0.8f, g = 0.8f, b = 0.8f;
        if (Profile.Milliseconds > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

        RenderText(text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }
}

void UStatOverlay::RenderShadowInfo()
{
    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Picking)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Decal))  OffsetY += 40.0f;

    float CurrentY = OverlayY + OffsetY;
    constexpr float LineHeight = 20.0f;

    // 라이트 개수별 정보
    {
        char Buf[256];
        (void)sprintf_s(Buf, sizeof(Buf), "Lights: Directional %u, Point %u, Spot %u, Ambient %u",
            DirectionalLightCount, PointLightCount, SpotLightCount, AmbientLightCount);
        FString Text = Buf;
        RenderText(Text, OverlayX, CurrentY, 1.0f, 0.8f, 0.0f);
        CurrentY += LineHeight;
    }

    // 총 라이트 개수
    {
        const uint32 TotalLights = DirectionalLightCount + PointLightCount + SpotLightCount + AmbientLightCount;
        char Buf[128];
        (void)sprintf_s(Buf, sizeof(Buf), "Total Lights: %u", TotalLights);
        FString Text = Buf;
        RenderText(Text, OverlayX, CurrentY, 1.0f, 0.8f, 0.0f);
        CurrentY += LineHeight;
    }

    // 섀도우맵 메모리 사용량
    {
        const float ShadowMemoryMB = static_cast<float>(ShadowMapMemoryBytes) / (1024.0f * 1024.0f);
        char Buf[128];
        (void)sprintf_s(Buf, sizeof(Buf), "Shadow Map Memory: %.2f MB", ShadowMemoryMB);
        FString Text = Buf;

        float r = 1.0f, g = 0.8f, b = 0.0f;
        if (ShadowMemoryMB > 500.0f)
        {
            r = 1.0f; g = 0.0f; b = 0.0f;
        }
        else if (ShadowMemoryMB > 200.0f)
        {
            r = 1.0f; g = 1.0f; b = 0.0f;
        }

        RenderText(Text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }

    // 렌더 타겟 메모리 사용량
    {
        const float RenderTargetMemoryMB = static_cast<float>(RenderTargetMemoryBytes) / (1024.0f * 1024.0f);
        char Buf[128];
        (void)sprintf_s(Buf, sizeof(Buf), "Render Target Memory: %.2f MB", RenderTargetMemoryMB);
        FString Text = Buf;

        float r = 1.0f, g = 0.8f, b = 0.0f;
        if (RenderTargetMemoryMB > 200.0f)
        {
            r = 1.0f; g = 0.0f; b = 0.0f;
        }
        else if (RenderTargetMemoryMB > 100.0f)
        {
            r = 1.0f; g = 1.0f; b = 0.0f;
        }

        RenderText(Text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }

    // Shadow 전용 GPU 메모리 사용량
    {
        const float TotalGPUMemoryMB = static_cast<float>(ShadowMapMemoryBytes + RenderTargetMemoryBytes) / (1024.0f * 1024.0f);
        char Buf[128];
        (void)sprintf_s(Buf, sizeof(Buf), "Shadow GPU Memory: %.2f MB", TotalGPUMemoryMB);
        FString Text = Buf;

        float r = 0.5f, g = 1.0f, b = 0.5f;
        if (TotalGPUMemoryMB > 700.0f)
        {
            r = 1.0f; g = 0.0f; b = 0.0f;
        }
        else if (TotalGPUMemoryMB > 300.0f)
        {
            r = 1.0f; g = 1.0f; b = 0.0f;
        }

        RenderText(Text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }

    // Shadow Atlas 사용 현황
    {
        char Buf[128];
        (void)sprintf_s(Buf, sizeof(Buf), "Atlas Tiles: %u / %u", UsedAtlasTiles, MaxAtlasTiles);
        FString Text = Buf;

        float r = 0.5f, g = 1.0f, b = 0.5f;
        if (MaxAtlasTiles > 0)
        {
            float UsageRatio = static_cast<float>(UsedAtlasTiles) / static_cast<float>(MaxAtlasTiles);
            if (UsageRatio > 0.9f)
            {
                r = 1.0f; g = 0.0f; b = 0.0f;
            }
            else if (UsageRatio > 0.7f)
            {
                r = 1.0f; g = 1.0f; b = 0.0f;
            }
        }

        RenderText(Text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }

    // CSM (Cascade Shadow Map) 정보
    if (DirectionalLightCount > 0)
    {
        UCascadeManager& CascadeMgr = UCascadeManager::GetInstance();

        // Cascade Split Number
        {
            char Buf[128];
            (void)sprintf_s(Buf, sizeof(Buf), "CSM Cascades: %d", CascadeMgr.GetSplitNum());
            FString Text = Buf;
            RenderText(Text, OverlayX, CurrentY, 0.0f, 1.0f, 1.0f);
            CurrentY += LineHeight;
        }

        // Distribution Factor
        {
            char Buf[128];
            (void)sprintf_s(Buf, sizeof(Buf), "CSM Distribution: %.2f", CascadeMgr.GetSplitBlendFactor());
            FString Text = Buf;
            RenderText(Text, OverlayX, CurrentY, 0.0f, 1.0f, 1.0f);
            CurrentY += LineHeight;
        }

        // Near Bias
        {
            char Buf[128];
            (void)sprintf_s(Buf, sizeof(Buf), "CSM Near Bias: %.1f", CascadeMgr.GetLightViewVolumeZNearBias());
            FString Text = Buf;
            RenderText(Text, OverlayX, CurrentY, 0.0f, 1.0f, 1.0f);
            CurrentY += LineHeight;
        }
    }
}

void UStatOverlay::RenderText(const FString& Text, float x, float y, float r, float g, float b)
{
    if (Text.empty())
    {
        return;
    }

    wstring WideText = ToWString(Text);

    FD2DOverlayManager& D2DManager = FD2DOverlayManager::GetInstance();

    D2D1_RECT_F Rect = D2D1::RectF(x, y, x + 800.0f, y + 20.0f);
    D2D1_COLOR_F color = D2D1::ColorF(r, g, b);

    // 반투명 회색 배경
    constexpr float PaddingLeft = 2.0f;
    constexpr float PaddingTop = 1.0f;
    constexpr float PaddingBottom = 1.0f;
    D2D1_RECT_F BGRect = D2D1::RectF(x - PaddingLeft, y - PaddingTop, x + 560.0f, y + 20.0f + PaddingBottom);
    const D2D1_COLOR_F ColorGrayBG = D2D1::ColorF(0.2f, 0.2f, 0.2f, 0.8f);
    D2DManager.AddRectangle(BGRect, ColorGrayBG, true);

    D2DManager.AddText(WideText.c_str(), Rect, color, 15.0f, false, false, L"Consolas");
}

wstring UStatOverlay::ToWString(const FString& InStr)
{
    if (InStr.empty())
    {
        return {};
    }

    int SizeNeeded = MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), static_cast<int>(InStr.size()), nullptr, 0);
    wstring WideStr(SizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), static_cast<int>(InStr.size()), WideStr.data(), SizeNeeded);
    return WideStr;
}

void UStatOverlay::EnableStat(EStatType type) { StatMask |= static_cast<uint8>(type); }
void UStatOverlay::DisableStat(EStatType type) { StatMask &= ~static_cast<uint8>(type); }
void UStatOverlay::SetStatType(EStatType type) { StatMask = static_cast<uint8>(type); }
bool UStatOverlay::IsStatEnabled(EStatType type) const
{
	const uint8 TypeMask = static_cast<uint8>(type);
	if (type == EStatType::All)
	{
		return (StatMask & TypeMask) == TypeMask;
	}
	return (StatMask & TypeMask) != 0;
}

void UStatOverlay::RecordPickingStats(float elapsedMs)
{
    ++PickAttempts;
    LastPickingTimeMs = elapsedMs;
    AccumulatedPickingTimeMs += elapsedMs;
}

void UStatOverlay::RecordDecalStats(uint32 InRenderedDecal, uint32 InCollidedCompCount)
{
    RenderedDecal = InRenderedDecal;
    CollidedCompCount = InCollidedCompCount;
}

void UStatOverlay::RecordShadowStats(uint32 InDirectionalLightCount, uint32 InPointLightCount, uint32 InSpotLightCount, uint32 InAmbientLightCount, uint64 InShadowMapMemoryBytes, uint64 InRenderTargetMemoryBytes, uint32 InUsedAtlasTiles, uint32 InMaxAtlasTiles)
{
    DirectionalLightCount = InDirectionalLightCount;
    PointLightCount = InPointLightCount;
    SpotLightCount = InSpotLightCount;
    AmbientLightCount = InAmbientLightCount;
    ShadowMapMemoryBytes = InShadowMapMemoryBytes;
    RenderTargetMemoryBytes = InRenderTargetMemoryBytes;
    UsedAtlasTiles = InUsedAtlasTiles;
    MaxAtlasTiles = InMaxAtlasTiles;
}
