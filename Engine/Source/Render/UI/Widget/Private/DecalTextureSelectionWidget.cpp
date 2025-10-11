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

    if (ImGui::CollapsingHeader("Fade Controls"))
    {
        if (ImGui::Button("Fade In"))
        {
            DecalComponent->BeginFadeIn();
        }
        ImGui::SameLine();
        if (ImGui::Button("Fade Out"))
        {
            DecalComponent->BeginFade();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause"))
        {
            DecalComponent->PauseFade();
        }
        ImGui::SameLine();
        if (ImGui::Button("Resume"))
        {
            DecalComponent->ResumeFade();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            DecalComponent->StopFade();
        }

        float fadeProgress = DecalComponent->GetFadeProgress();
        ImGui::ProgressBar(1.0f - fadeProgress, ImVec2(-1.0f, 0.0f));

        ImGui::PushItemWidth(120.0f);

        float fadeInStartDelay = DecalComponent->GetFadeInStartDelay();
        if (ImGui::InputFloat("Fade In Start Delay", &fadeInStartDelay))
        {
            DecalComponent->SetFadeInStartDelay(fadeInStartDelay);
        }

        float fadeInDuration = DecalComponent->GetFadeInDuration();
        if (ImGui::InputFloat("Fade In Duration", &fadeInDuration))
        {
            DecalComponent->SetFadeInDuration(fadeInDuration);
        }

        float fadeStartDelay = DecalComponent->GetFadeStartDelay();
        if (ImGui::InputFloat("Fade Out Start Delay", &fadeStartDelay))
        {
            DecalComponent->SetFadeStartDelay(fadeStartDelay);
        }

        float fadeDuration = DecalComponent->GetFadeDuration();
        if (ImGui::InputFloat("Fade Out Duration", &fadeDuration))
        {
            DecalComponent->SetFadeDuration(fadeDuration);
        }
        ImGui::PopItemWidth();

        bool bDestroyOwner = DecalComponent->GetDestroyOwnerAfterFade();
        if (ImGui::Checkbox("Destroy Owner After Fade", &bDestroyOwner))
        {
            DecalComponent->SetDestroyOwnerAfterFade(bDestroyOwner);
        }
    }

    ImGui::Separator();
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
