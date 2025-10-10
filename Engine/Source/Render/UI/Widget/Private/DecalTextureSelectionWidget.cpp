#include "pch.h"
#include "Render/UI//Widget/Public/DecalTextureSelectionWidget.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/DecalComponent.h"

#include "Level/Public/Level.h"
#include "Core/Public/ObjectIterator.h"
#include "Texture/Public/Material.h"
#include "Texture/Public/Texture.h"
#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"


IMPLEMENT_CLASS(UDecalTextureSelectionWidget, UWidget)

void UDecalTextureSelectionWidget::Initialize()
{

}

void UDecalTextureSelectionWidget::Update()
{
}

void UDecalTextureSelectionWidget::RenderWidget()
{
	if (!DecalComponent)
		return;

	ImGui::Separator();
	ImGui::Text("Select Sprite");

	ImGui::Spacing();
	
	static int current_item = 0; // ���� ���õ� �ε���
	
	// ���� ���ڿ� ���
	TArray<FString> items;
	const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
		UAssetManager::GetInstance().GetTextureCache();
	
	int i = 0;
	for (auto Itr = TextureCache.begin(); Itr != TextureCache.end(); Itr++, i++)
	{
		if (Itr->first == DecalComponent->GetSprite().first)
			current_item = i;
	
		items.push_back(Itr->first.ToString());
	}
	
	sort(items.begin(), items.end());
	
	if (ImGui::BeginCombo("Sprite", items[current_item].c_str())) // Label�� ���� �� ǥ��
	{
		for (int n = 0; n < items.size(); n++)
		{
			bool is_selected = (current_item == n);
			if (ImGui::Selectable(items[n].c_str(), is_selected))
			{
				current_item = n;
				SetSpriteOfActor(items[current_item]);
			}
	
			if (is_selected)
				ImGui::SetItemDefaultFocus(); // �⺻ ��Ŀ��
		}
		ImGui::EndCombo();
	}
	
	ImGui::Separator();
	
	WidgetNum = (WidgetNum + 1) % std::numeric_limits<uint32>::max();
}

UDecalTextureSelectionWidget::UDecalTextureSelectionWidget() : UWidget("Decal Texture Selection Widget")
{
}

UDecalTextureSelectionWidget::~UDecalTextureSelectionWidget() = default;

void UDecalTextureSelectionWidget::RenderMaterialSections()
{

}

void UDecalTextureSelectionWidget::RenderAvailableMaterials(int32 TargetSlotIdex)
{
}

void UDecalTextureSelectionWidget::RenderOptions()
{

}

FString UDecalTextureSelectionWidget::GetMaterialDisplayName(UMaterial* Material) const
{
	return FString();
}
