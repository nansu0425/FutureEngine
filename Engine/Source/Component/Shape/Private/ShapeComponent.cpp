#include "pch.h"
#include "Component/Shape/Public/ShapeComponent.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(UShapeComponent, UPrimitiveComponent)

UShapeComponent::UShapeComponent()
{
	// Shape Component는 기본적으로 overlap 이벤트를 생성
	bGenerateOverlapEvents = true;

	// 기본적으로 보이지 않음 (디버그용)
	bVisible = false;
}

UShapeComponent::~UShapeComponent()
{
}

void UShapeComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// Shape Color
		if (InOutHandle.hasKey("ShapeColor"))
		{
			JSON& ColorHandle = InOutHandle["ShapeColor"];
			ShapeColor.X = static_cast<float>(ColorHandle["R"].ToFloat());
			ShapeColor.Y = static_cast<float>(ColorHandle["G"].ToFloat());
			ShapeColor.Z = static_cast<float>(ColorHandle["B"].ToFloat());
			ShapeColor.W = static_cast<float>(ColorHandle["A"].ToFloat());
		}

		// Draw Only If Selected
		FString DrawOnlyIfSelectedString;
		FJsonSerializer::ReadString(InOutHandle, "bDrawOnlyIfSelected", DrawOnlyIfSelectedString, "false");
		bDrawOnlyIfSelected = (DrawOnlyIfSelectedString == "true");

		// Line Thickness
		if (InOutHandle.hasKey("LineThickness"))
		{
			LineThickness = static_cast<float>(InOutHandle["LineThickness"].ToFloat());
		}
	}
	else
	{
		// Shape Color
		JSON ColorHandle = JSON::Make(JSON::Class::Object);
		ColorHandle["R"] = ShapeColor.X;
		ColorHandle["G"] = ShapeColor.Y;
		ColorHandle["B"] = ShapeColor.Z;
		ColorHandle["A"] = ShapeColor.W;
		InOutHandle["ShapeColor"] = ColorHandle;

		// Draw Only If Selected
		InOutHandle["bDrawOnlyIfSelected"] = bDrawOnlyIfSelected ? "true" : "false";

		// Line Thickness
		InOutHandle["LineThickness"] = LineThickness;
	}
}
