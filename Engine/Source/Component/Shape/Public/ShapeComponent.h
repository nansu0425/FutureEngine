#pragma once
#include "Component/Public/PrimitiveComponent.h"

/**
 * @brief Shape Component의 기본 클래스
 * Box, Sphere, Capsule 등의 기하학적 충돌 형상을 표현
 */
UCLASS()
class UShapeComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

public:
	UShapeComponent();
	virtual ~UShapeComponent() override;

	// Shape Color (RGBA, 0-255 range)
	FVector4 GetShapeColor() const { return ShapeColor; }
	void SetShapeColor(const FVector4& InColor) { ShapeColor = InColor; }

	// Draw Settings
	bool IsDrawOnlyIfSelected() const { return bDrawOnlyIfSelected; }
	void SetDrawOnlyIfSelected(bool bInDrawOnlyIfSelected) { bDrawOnlyIfSelected = bInDrawOnlyIfSelected; }

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// Duplication
	virtual UObject* Duplicate() override;

protected:
	// Shape 렌더링 색상 (RGBA, 0-255) - 보라색
	FVector4 ShapeColor = FVector4(200.f, 0.f, 255.f, 255.f);

	// 선택된 경우에만 그리기
	bool bDrawOnlyIfSelected = false;
};
