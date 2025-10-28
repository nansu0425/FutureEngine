#pragma once

#include "Component/Public/PrimitiveComponent.h"
#include "Global/Vector.h"

/**
 * @brief 에디터 전용 아이콘 컴포넌트
 * BillBoardComponent와 달리 에디터 Billboard 플래그와 무관하게 항상 렌더링
 * PIE에서는 렌더링되지 않음
 */
UCLASS()
class UEditorIconComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UEditorIconComponent, UPrimitiveComponent)

public:
	UEditorIconComponent();
	virtual ~UEditorIconComponent() override;

	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	void FaceCamera(const FVector& CameraForward);

	UTexture* GetSprite() const;
	void SetSprite(UTexture* Sprite);

	FVector4 GetSpriteTint() const { return SpriteTint; }
	void SetSpriteTint(const FVector4& InTint);
	void SetSpriteTint(const FVector& InTint, float Alpha = 1.0f);

	UClass* GetSpecificWidgetClass() const override;

	static const FRenderState& GetClassDefaultRenderState();

private:
	UTexture* Sprite = nullptr;
	FVector4 SpriteTint = FVector4::OneVector();

// Screen Size Section
public:
	bool IsScreenSizeScaled() const { return bScreenSizeScaled; }
	float GetScreenSize() const { return ScreenSize; }
	void SetScreenSizeScaled(bool bEnable, float InScreenSize = 0.1f)
	{
		bScreenSizeScaled = bEnable;
		ScreenSize = InScreenSize;
	}

private:
	bool bScreenSizeScaled = false;
	float ScreenSize = 0.1f;
};
