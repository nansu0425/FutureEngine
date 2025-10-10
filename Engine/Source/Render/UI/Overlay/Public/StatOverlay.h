#pragma once
#include "Core/Public/Object.h"
#include <d2d1.h>
#include <dwrite.h>

enum class EStatType : uint8
{
	None = 0,
	FPS = 1 << 0,      // 1
	Memory = 1 << 1,   // 2
	Picking = 1 << 2,  // 4
	Time = 1 << 3,  // 8
	All = FPS | Memory | Picking | Time
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

	// Stat control methods
	void ShowFPS(bool bShow) { bShow ? EnableStat(EStatType::FPS) : DisableStat(EStatType::FPS); }
	void ShowMemory(bool bShow) { bShow ? EnableStat(EStatType::Memory) : DisableStat(EStatType::Memory); }
	void ShowPicking(bool bShow) { bShow ? EnableStat(EStatType::Picking) : DisableStat(EStatType::Picking); }
	void ShowTime(bool bShow) { bShow ? EnableStat(EStatType::Time) : DisableStat(EStatType::Time); }
	void ShowAll(bool bShow) { SetStatType(bShow ? EStatType::All : EStatType::None); }

	// API to update stats
	void RecordPickingStats(float ElapsedMS);

private:
	void RenderFPS(ID2D1DeviceContext* d2dCtx);
	void RenderMemory(ID2D1DeviceContext* d2dCtx);
	void RenderPicking(ID2D1DeviceContext* d2dCtx);
	void RenderTimeInfo(ID2D1DeviceContext* d2dCtx);
	void RenderText(ID2D1DeviceContext* d2dCtx, const FString& Text, float X, float Y, float R, float G, float B);
	template <typename T>
	inline void SafeRelease(T*& ptr)
	{
		if (ptr)
		{
			ptr->Release();
			ptr = nullptr;
		}
	}

	// FPS Stats
	float CurrentFPS = 0.0f;
	float FrameTime = 0.0f;

	// Picking Stats
	uint32 PickAttempts = 0;
	float LastPickingTimeMs = 0.0f;
	float AccumulatedPickingTimeMs = 0.0f;

	// Rendering position
	float OverlayX = 18.0f;
	float OverlayY = 55.0f;

	uint8 StatMask = static_cast<uint8>(EStatType::None);

	// Helper methods
	std::wstring ToWString(const FString& InStr);
	void EnableStat(EStatType InStatType);
	void DisableStat(EStatType InStatType);
	void SetStatType(EStatType InStatType);
	bool IsStatEnabled(EStatType InStatType) const;

	IDWriteTextFormat* TextFormat = nullptr;
	
	IDWriteFactory* DWriteFactory = nullptr;
};
