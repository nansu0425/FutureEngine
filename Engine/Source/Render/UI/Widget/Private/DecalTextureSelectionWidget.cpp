#include "pch.h"
#include "Render/UI/Widget/Public/DecalTextureSelectionWidget.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Component/Public/ActorComponent.h"
#include "ImGui/imgui.h"
#include "Level/Public/Level.h"
#include <filesystem>

IMPLEMENT_CLASS(UDecalTextureSelectionWidget, UWidget)

void UDecalTextureSelectionWidget::Initialize()
{
}

void UDecalTextureSelectionWidget::Update()
{
    // �� ������ Level�� ���õ� Actor�� Ȯ���ؼ� ���� �ݿ�
    ULevel* CurrentLevel = GWorld->GetLevel();

    if (CurrentLevel)
    {
        UActorComponent* NewSelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();

        // Update Current Selected Actor
        if (DecalComponent != NewSelectedComponent)
        {
            DecalComponent = Cast<UDecalComponent>(NewSelectedComponent);

        }
    }
}

void UDecalTextureSelectionWidget::RenderWidget()
{
    if (!DecalComponent)
    {
        return;
    }

    ImGui::Separator();

    // 좌측 미니 프리뷰 (정사각형)
    const float PreviewSize = 72.0f;
    ID3D11ShaderResourceView* SRV = nullptr;
    UTexture* CurrentTexture = DecalComponent->GetTexture();
    if (CurrentTexture)
    {
        SRV = UAssetManager::GetInstance().GetTexture(CurrentTexture->GetFilePath()).Get();
    }

    if (SRV)
    {
        ImGui::Image((ImTextureID)SRV, ImVec2(PreviewSize, PreviewSize), ImVec2(0, 0), ImVec2(1, 1));
    }
    else
    {
        ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));
    }
    ImGui::SameLine();

    // 현재 스프라이트 경로 표시 (파일명만)
    FString CurrentPath;
    if (CurrentTexture)
    {
        CurrentPath = CurrentTexture->GetFilePath().ToString();
    }

    FString Preview = "<None>";
    if (!CurrentPath.empty())
    {
        Preview = std::filesystem::path(CurrentPath).stem().string();
    }

    if (ImGui::BeginCombo("Texture (png)##Combo", Preview.c_str()))
    {
        const TMap<FName, ID3D11ShaderResourceView*>& textureCache = UAssetManager::GetInstance().GetTextureCache();
        for (auto const& [Path, textureView] : textureCache)
        {
            const FString PathStr = Path.ToString();
            const FString DisplayName = std::filesystem::path(PathStr).stem().string();
            bool bSelected = (PathStr == CurrentPath);
            if (ImGui::Selectable(DisplayName.c_str(), bSelected))
            {
                if (!bSelected)
                {
                    UTexture* NewTexture = UAssetManager::GetInstance().CreateTexture(Path);
                    DecalComponent->SetTexture(NewTexture);
                }
            }
            if (bSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // 1. Get the current state from the component
    bool bPerspective = DecalComponent->IsPerspective();

    // 2. Draw the checkbox. The 'if' statement is true only when the user clicks it.
    if (ImGui::Checkbox("Perspective", &bPerspective))
    {
        // 3. If changed, update the component's state
        DecalComponent->SetPerspective(bPerspective);
    }

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
