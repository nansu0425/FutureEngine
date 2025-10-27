#include "pch.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Global/Types.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Global/Memory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/D2DOverlayManager.h"

IMPLEMENT_SINGLETON_CLASS(UStatOverlay, UObject)

UStatOverlay::UStatOverlay() {}
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
}

void UStatOverlay::RenderFPS()
{
    auto& timeManager = UTimeManager::GetInstance();
    CurrentFPS = timeManager.GetFPS();
    FrameTime = timeManager.GetDeltaTime() * 1000;

    char buf[64];
    sprintf_s(buf, sizeof(buf), "FPS: %.1f (%.2f ms)", CurrentFPS, FrameTime);
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
    sprintf_s(Buf, sizeof(Buf), "Memory: %.1f MB (%u objects)", MemoryMB, TotalAllocationCount);
    FString text = Buf;

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    RenderText(text, OverlayX, OverlayY + OffsetY, 1.0f, 1.0f, 0.0f);
}

void UStatOverlay::RenderPicking()
{
    float AvgMs = PickAttempts > 0 ? AccumulatedPickingTimeMs / PickAttempts : 0.0f;

    char Buf[128];
    sprintf_s(Buf, sizeof(Buf), "Picking Time %.2f ms (Attempts %u, Accum %.2f ms, Avg %.2f ms)",
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
    {
        char Buf[128];
        sprintf_s(Buf, sizeof(Buf), "Rendered Decal: %d (Collided Components: %d)",
            RenderedDecal, CollidedCompCount);
        FString Text = Buf;

        float OffsetY = 0.0f;
        if (IsStatEnabled(EStatType::FPS))      OffsetY += 20.0f;
        if (IsStatEnabled(EStatType::Memory))   OffsetY += 20.0f;
        if (IsStatEnabled(EStatType::Picking))  OffsetY += 20.0f;

        RenderText(Text, OverlayX, OverlayY + OffsetY, 0.f, 1.f, 0.f);
    }

    {
        char Buf[128];
        sprintf_s(Buf, sizeof(Buf), "Decal Pass Time: %.4f ms", FScopeCycleCounter::GetTimeProfile("DecalPass").Milliseconds);
        FString Text = Buf;

        float OffsetY = 20.0f;
        if (IsStatEnabled(EStatType::FPS))      OffsetY += 20.0f;
        if (IsStatEnabled(EStatType::Memory))   OffsetY += 20.0f;
        if (IsStatEnabled(EStatType::Picking))  OffsetY += 20.0f;
        RenderText(Text, OverlayX, OverlayY + OffsetY, 0.f, 1.f, 0.f);
    }
}

void UStatOverlay::RenderTimeInfo()
{
    const TArray<FString> ProfileKeys = FScopeCycleCounter::GetTimeProfileKeys();

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Picking)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Decal))  OffsetY += 40.0f;

    float CurrentY = OverlayY + OffsetY;
    const float LineHeight = 20.0f;

    for (const FString& Key : ProfileKeys)
    {
        const FTimeProfile& Profile = FScopeCycleCounter::GetTimeProfile(Key);

        char buf[128];
        sprintf_s(buf, sizeof(buf), "%s: %.2f ms", Key.c_str(), Profile.Milliseconds);
        FString text = buf;

        float r = 0.8f, g = 0.8f, b = 0.8f;
        if (Profile.Milliseconds > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

        RenderText(text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }
}

void UStatOverlay::RenderText(const FString& Text, float x, float y, float r, float g, float b)
{
    if (Text.empty()) return;

    std::wstring wText = ToWString(Text);

    FD2DOverlayManager& D2DManager = FD2DOverlayManager::GetInstance();

    D2D1_RECT_F rect = D2D1::RectF(x, y, x + 800.0f, y + 20.0f);
    D2D1_COLOR_F color = D2D1::ColorF(r, g, b);

    D2DManager.AddText(wText.c_str(), rect, color, 15.0f, false, false, L"Consolas");
}

std::wstring UStatOverlay::ToWString(const FString& InStr)
{
    if (InStr.empty()) return std::wstring();

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), NULL, 0);
    std::wstring wStr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), &wStr[0], sizeNeeded);
    return wStr;
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
