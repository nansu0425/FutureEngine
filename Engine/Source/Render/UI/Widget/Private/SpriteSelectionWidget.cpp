#include "pch.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"

#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Component/Public/BillBoardComponent.h"

#include <climits>

IMPLEMENT_CLASS(USpriteSelectionWidget, UWidget)

USpriteSelectionWidget::USpriteSelectionWidget()
	: UWidget("Sprite Selection Widget")
{
}

USpriteSelectionWidget::~USpriteSelectionWidget() = default;

void USpriteSelectionWidget::Initialize()
{
	// Do Nothing Here
}

void USpriteSelectionWidget::Update()
{
	// �� ������ Level�� ���õ� Actor�� Ȯ���ؼ� ���� �ݿ�
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (CurrentLevel)
	{
		AActor* NewSelectedActor = GEditor->GetEditorModule()->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != NewSelectedActor)
		{
			SelectedActor = NewSelectedActor;

			for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
			{
				if (UBillBoardComponent* UUIDTextComponent = Cast<UBillBoardComponent>(Component))
					{ SelectedBillBoard = UUIDTextComponent; }
			}
		}
	}
}

void USpriteSelectionWidget::RenderWidget()
{
	if (!SelectedActor)
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
		if (Itr->first == SelectedBillBoard->GetSprite().first)
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

void USpriteSelectionWidget::SetSpriteOfActor(FString NewSprite)
{
	if (!SelectedActor)
		return;
	if (!SelectedBillBoard)
		return;

	const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
		UAssetManager::GetInstance().GetTextureCache();

	SelectedBillBoard->SetSprite(*TextureCache.find(NewSprite));
}
