#pragma once
#include "Render/UI/Widget/Public/Widget.h"

class SSplitter;

class USplitterDebugWidget : public UWidget
{
public:
	USplitterDebugWidget() = default;
	virtual ~USplitterDebugWidget() override;

	void Initialize() override {}
	void Update() override {}
	void RenderWidget() override;

	void SetSplitters(const SSplitter* InRoot, const SSplitter* InLeft, const SSplitter* InRight)
	{
		RootSplitter = InRoot;
		LeftSplitter = InLeft;
		RightSplitter = InRight;
	}

private:
	const SSplitter* RootSplitter = nullptr;
	const SSplitter* LeftSplitter = nullptr;
	const SSplitter* RightSplitter = nullptr;
};
