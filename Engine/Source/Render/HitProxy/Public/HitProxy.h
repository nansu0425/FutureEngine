#pragma once

class UPrimitiveComponent;

// 기즈모 축 타입
enum class EGizmoAxisType : uint8
{
	None = 0,
	X = 1,
	Y = 2,
	Z = 3
};

// HitProxy ID (RGB 값으로 인코딩)
struct FHitProxyId
{
	uint32 Index;  // RGB를 uint32로 변환 (R << 16 | G << 8 | B)

	FHitProxyId() : Index(0) {}
	explicit FHitProxyId(uint32 InIndex) : Index(InIndex) {}
	FHitProxyId(uint8 R, uint8 G, uint8 B)
		: Index((static_cast<uint32>(R) << 16) | (static_cast<uint32>(G) << 8) | static_cast<uint32>(B))
	{
	}

	bool IsValid() const
	{
		return Index != 0;  // Index 0은 배경(검은색)
	}

	FVector4 GetColor() const
	{
		uint8 R = (Index >> 16) & 0xFF;
		uint8 G = (Index >> 8) & 0xFF;
		uint8 B = (Index >> 0) & 0xFF;
		return FVector4(R / 255.0f, G / 255.0f, B / 255.0f, 1.0f);
	}

	bool operator==(const FHitProxyId& Other) const
	{
		return Index == Other.Index;
	}

	bool operator!=(const FHitProxyId& Other) const
	{
		return Index != Other.Index;
	}
};

// Invalid HitProxy ID
static const FHitProxyId InvalidHitProxyId = FHitProxyId(0);

// HitProxy 기본 클래스
class HHitProxy
{
public:
	FHitProxyId Id;

	HHitProxy(FHitProxyId InId) : Id(InId) {}
	virtual ~HHitProxy() = default;

	virtual bool IsWidgetAxis() const { return false; }
	virtual bool IsComponent() const { return false; }
};

// 기즈모 축 HitProxy
class HWidgetAxis : public HHitProxy
{
public:
	EGizmoAxisType Axis;

	HWidgetAxis(EGizmoAxisType InAxis, FHitProxyId InId)
		: HHitProxy(InId), Axis(InAxis)
	{
	}

	virtual bool IsWidgetAxis() const override { return true; }
};

// 컴포넌트 HitProxy
class HComponent : public HHitProxy
{
public:
	UPrimitiveComponent* Component;

	HComponent(UPrimitiveComponent* InComponent, FHitProxyId InId)
		: HHitProxy(InId), Component(InComponent)
	{
	}

	virtual bool IsComponent() const override { return true; }
};

// HitProxy 관리자 (싱글톤)
class FHitProxyManager
{
public:
	static FHitProxyManager& GetInstance()
	{
		static FHitProxyManager Instance;
		return Instance;
	}

	// HitProxy 할당 및 ID 반환
	FHitProxyId AllocateHitProxyId(HHitProxy* HitProxy)
	{
		if (!HitProxy)
		{
			return InvalidHitProxyId;
		}

		// 새로운 ID 할당 (1부터 시작, 0은 배경)
		uint32 NewIndex = NextIndex++;
		FHitProxyId NewId(NewIndex);
		HitProxy->Id = NewId;

		// 맵에 등록
		HitProxyMap[NewIndex] = HitProxy;
		return NewId;
	}

	// ID로 HitProxy 조회
	HHitProxy* GetHitProxy(FHitProxyId Id) const
	{
		auto It = HitProxyMap.find(Id.Index);
		if (It != HitProxyMap.end())
		{
			return It->second;
		}
		return nullptr;
	}

	// 모든 HitProxy 제거 (프레임 시작 시 호출)
	void ClearAllHitProxies()
	{
		for (auto& Pair : HitProxyMap)
		{
			delete Pair.second;
		}
		HitProxyMap.clear();
		NextIndex = 1;  // 0은 배경
	}

	~FHitProxyManager()
	{
		ClearAllHitProxies();
	}

private:
	FHitProxyManager() : NextIndex(1) {}  // 0은 배경용
	FHitProxyManager(const FHitProxyManager&) = delete;
	FHitProxyManager& operator=(const FHitProxyManager&) = delete;

	std::unordered_map<uint32, HHitProxy*> HitProxyMap;
	uint32 NextIndex;
};
