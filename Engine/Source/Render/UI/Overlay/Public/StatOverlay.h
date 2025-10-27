#pragma once
#include "Core/Public/Object.h"
#include <d2d1.h>
#include <dwrite.h>

enum class EStatType : uint8
{
	None =		0,		 // 0
	FPS =		1 << 0,  // 1
	Memory =	1 << 1,  // 2
	Picking =	1 << 2,  // 4
	Decal =		1 << 3,  // 8
	Time =		1 << 4,	 // 16
	All = FPS | Memory | Picking | Time | Decal
};

UCLASS()
class UStatOverlay : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UStatOverlay, UObject)

public:
	void Initialize();
	void Release();
	void Render();

	// Stat control methods (토글)
	void ToggleFPS() { IsStatEnabled(EStatType::FPS) ? DisableStat(EStatType::FPS) : EnableStat(EStatType::FPS); }
	void ToggleMemory() { IsStatEnabled(EStatType::Memory) ? DisableStat(EStatType::Memory) : EnableStat(EStatType::Memory); }
	void TogglePicking() { IsStatEnabled(EStatType::Picking) ? DisableStat(EStatType::Picking) : EnableStat(EStatType::Picking); }
	void ToggleTime() { IsStatEnabled(EStatType::Time) ? DisableStat(EStatType::Time) : EnableStat(EStatType::Time); }
	void ToggleDecal() { IsStatEnabled(EStatType::Decal) ? DisableStat(EStatType::Decal) : EnableStat(EStatType::Decal); }
	void ToggleAll() { IsStatEnabled(EStatType::All) ? DisableStat(EStatType::All) : EnableStat(EStatType::All); }

	// Stat control methods (명시적 켜기/끄기)
	void ShowFPS() { EnableStat(EStatType::FPS); }
	void ShowMemory() { EnableStat(EStatType::Memory); }
	void ShowPicking() { EnableStat(EStatType::Picking); }
	void ShowTime() { EnableStat(EStatType::Time); }
	void ShowDecal() { EnableStat(EStatType::Decal); }
	void ShowAll() { EnableStat(EStatType::All); }
	void HideAll() { SetStatType(EStatType::None); }

	// API to update stats
	void RecordPickingStats(float ElapsedMS);
	void RecordDecalStats(uint32 InRenderedDecal, uint32 InCollidedCompCount);
	
private:
	void RenderFPS();
	void RenderMemory();
	void RenderPicking();
	void RenderDecalInfo();
	void RenderTimeInfo();
	void RenderText(const FString& Text, float X, float Y, float R, float G, float B);

	// FPS Stats
	float CurrentFPS = 0.0f;
	float FrameTime = 0.0f;

	// Picking Stats
	uint32 PickAttempts = 0;
	float LastPickingTimeMs = 0.0f;
	float AccumulatedPickingTimeMs = 0.0f;

	// Decal Stats
	uint32 RenderedDecal = 0;
	uint32 CollidedCompCount = 0;

	// Rendering position
	float OverlayX = 18.0f;
	float OverlayY = 135.0f;

	uint8 StatMask = static_cast<uint8>(EStatType::None);

	// Helper methods
	std::wstring ToWString(const FString& InStr);
	void EnableStat(EStatType InStatType);
	void DisableStat(EStatType InStatType);
	void SetStatType(EStatType InStatType);
	bool IsStatEnabled(EStatType InStatType) const;
};
