#include "pch.h"
#include "Render/UI/Widget/Public/StaticMeshComponentWidget.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Mesh/Public/StaticMesh.h"

#include "Level/Public/Level.h"
#include "Core/Public/ObjectIterator.h"
#include "Texture/Public/Material.h"
#include "Texture/Public/Texture.h"

IMPLEMENT_CLASS(UStaticMeshComponentWidget, UWidget)

void UStaticMeshComponentWidget::RenderWidget()
{
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (!CurrentLevel)
	{
		ImGui::TextUnformatted("No Level Loaded");
		return;
	}

	UActorComponent* Component = GEditor->GetEditorModule()->GetSelectedComponent();
	if (!Component)
	{
		ImGui::TextUnformatted("No Object Selected");
		return;
	}
	StaticMeshComponent = Cast<UStaticMeshComponent>(Component);

	if (!StaticMeshComponent)
	{
		return;
	}

	if (!StaticMeshComponent->GetStaticMesh())
	{
		return;
	}

	// 모든 입력 필드를 검은색으로 설정
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
	
	RenderStaticMeshSelector();
	ImGui::Separator();
	RenderMaterialSections();
	ImGui::Separator();
	RenderOptions();
	
	ImGui::PopStyleColor(5);
}

void UStaticMeshComponentWidget::RenderStaticMeshSelector()
{
	// 1. 현재 컴포넌트에 할당된 스태틱 메시를 가져옵니다.
	UStaticMesh* CurrentStaticMesh = StaticMeshComponent->GetStaticMesh();
	const FName PreviewName = CurrentStaticMesh ? CurrentStaticMesh->GetAssetPathFileName() : "None";

	// 2. ImGui::BeginCombo를 사용하여 드롭다운 메뉴를 시작합니다.
	// 첫 번째 인자는 라벨, 두 번째 인자는 닫혀 있을 때 표시될 텍스트입니다.
	if (ImGui::BeginCombo("Static Mesh", PreviewName.ToString().c_str()))
	{
		// 3. TObjectIterator로 모든 UStaticMesh 에셋을 순회합니다.
		for (TObjectIterator<UStaticMesh> It; It; ++It)
		{
			UStaticMesh* MeshInList = *It;
			if (!MeshInList) continue;

			// 현재 선택된 항목인지 확인합니다.
			const bool bIsSelected = (CurrentStaticMesh == MeshInList);

			// 4. ImGui::Selectable로 각 항목을 만듭니다.
			// 사용자가 이 항목을 클릭하면 if문이 true가 됩니다.
			if (ImGui::Selectable(MeshInList->GetAssetPathFileName().ToString().c_str(), bIsSelected))
			{
				// 5. 항목이 선택되면, 컴포넌트의 스태틱 메시를 교체합니다.
				StaticMeshComponent->SetStaticMesh(MeshInList->GetAssetPathFileName());
			}

			// 현재 선택된 항목에 포커스를 맞춰서 드롭다운이 열렸을 때 바로 보이게 합니다.
			if (bIsSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}

		// 6. 드롭다운 메뉴를 닫습니다.
		ImGui::EndCombo();
	}
}

void UStaticMeshComponentWidget::RenderMaterialSections()
{
	UStaticMesh* CurrentMesh = StaticMeshComponent->GetStaticMesh();
	FStaticMesh* MeshAsset = CurrentMesh->GetStaticMeshAsset();

	ImGui::Text("Material Slots (%d)", static_cast<int>(MeshAsset->MaterialInfo.size()));

	// 머티리얼 슬롯
	for (int32 SlotIndex = 0; SlotIndex < MeshAsset->MaterialInfo.size(); ++SlotIndex)
	{
		// 현재 할당된 Material 가져오기 (OverrideMaterials 우선)
		UMaterial* CurrentMaterial = StaticMeshComponent->GetMaterial(SlotIndex);
		FString PreviewName = CurrentMaterial ? GetMaterialDisplayName(CurrentMaterial) : "None";
		// Material 정보 표시
		ImGui::PushID(SlotIndex);

		std::string Label = "Element " + std::to_string(SlotIndex);
		// “Element N”을 먼저 한 줄로 출력
		ImGui::TextUnformatted(Label.c_str());

		// 현재 슬롯의 머티리얼 썸네일(이전 단계에서 추가했던 코드 그대로 사용)
		const float PreviewSize = 64.0f;
		UTexture* MaterialPreviewTexture = GetPreviewTextureForMaterial(CurrentMaterial);
		ID3D11ShaderResourceView* ShaderResourceView = nullptr;
		if (MaterialPreviewTexture != nullptr)
		{
			ShaderResourceView = MaterialPreviewTexture->GetTextureSRV();
		}

		if (ShaderResourceView != nullptr)
		{
			ImGui::Image((ImTextureID)ShaderResourceView, ImVec2(PreviewSize, PreviewSize), ImVec2(0, 0), ImVec2(1, 1));
		}
		else
		{
			ImGui::Dummy(ImVec2(PreviewSize, PreviewSize));
		}

		// 썸네일과 콤보를 같은 라인에 유지
		ImGui::SameLine();

		// 콤보 폭을 남은 가로 영역으로
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

		// 보이는 라벨은 숨기고(“##”) 슬롯마다 고유 ID를 부여
		std::string ComboId = "##MaterialSlotCombo_" + std::to_string(SlotIndex);
		if (ImGui::BeginCombo(ComboId.c_str(), PreviewName.c_str()))
		{
			RenderAvailableMaterials(SlotIndex);
			ImGui::EndCombo();
		}
		FVector Ambient = CurrentMaterial->GetAmbientColor();
		FVector Diffuse = CurrentMaterial->GetDiffuseColor();
		FVector Specular = CurrentMaterial->GetSpecularColor();
		if (ImGui::ColorEdit3("Ambient", &Ambient.X))
		{
			CurrentMaterial->SetAmbientColor(Ambient);
		}
		if (ImGui::ColorEdit3("Diffuse", &Diffuse.X))
		{
			CurrentMaterial->SetDiffuseColor(Diffuse);
		}
		if (ImGui::ColorEdit3("Specular", &Specular.X))
		{
			CurrentMaterial->SetSpecularColor(Specular);
		}



		ImGui::PopID();
	}
}

void UStaticMeshComponentWidget::RenderAvailableMaterials(int32 TargetSlotIndex)
{
	// 모든 UMaterial 순회
	for (TObjectIterator<UMaterial> It; It; ++It)
	{
		UMaterial* Mat = *It;
		if (!Mat) continue;

		FString MatName = GetMaterialDisplayName(Mat);
		bool bIsSelected = (StaticMeshComponent->GetMaterial(TargetSlotIndex) == Mat);
		// 콤보 항목의 머티리얼 썸네일(작게)
		const float RowPreviewSize = 20.0f;
		UTexture* RowPreviewTexture = GetPreviewTextureForMaterial(Mat);
		ID3D11ShaderResourceView* RowShaderResourceView = nullptr;
		if (RowPreviewTexture != nullptr)
		{
			RowShaderResourceView = RowPreviewTexture->GetTextureSRV();
		}

		if (RowShaderResourceView != nullptr)
		{
			ImGui::Image((ImTextureID)RowShaderResourceView, 
				ImVec2(RowPreviewSize, RowPreviewSize), ImVec2(0, 0), ImVec2(1, 1));
		}
		else
		{
			ImGui::Dummy(ImVec2(RowPreviewSize, RowPreviewSize));
		}
		ImGui::SameLine();

		if (ImGui::Selectable(MatName.c_str(), bIsSelected))
		{
			// Material을 직접 참조
			StaticMeshComponent->SetMaterial(TargetSlotIndex, Mat);
		}

		if (bIsSelected)
		{
			ImGui::SetItemDefaultFocus();
		}
	}
}

void UStaticMeshComponentWidget::RenderOptions()
{
	bool bIsScrollEnabled = StaticMeshComponent->IsScrollEnabled();

	if (ImGui::Checkbox("Enable Scroll", &bIsScrollEnabled))
	{
		if (bIsScrollEnabled)
		{
			StaticMeshComponent->EnableScroll();
		}
		else
		{
			StaticMeshComponent->DisableScroll();
		}
	}
	bool IsNormalMapEnabledLocal = StaticMeshComponent->IsNormalMapEnabled();
	if (ImGui::Checkbox("Enable Normal Map", &IsNormalMapEnabledLocal))
	{
		if (IsNormalMapEnabledLocal)
		{
			StaticMeshComponent->EnableNormalMap();
		}
		else
		{
			StaticMeshComponent->DisableNormalMap();
		}
	}
}

FString UStaticMeshComponentWidget::GetMaterialDisplayName(UMaterial* Material) const
{
	if (!Material)
	{
		return "None";
	}

	// 1순위: UObject 이름 사용 (기본 "Object_" 형식이 아닌 경우)
	FString ObjectName = Material->GetName().ToString();
	if (!ObjectName.empty() && ObjectName.find("Object_") != 0)
	{
		return ObjectName;
	}

	// 2순위: Diffuse 텍스처 파일 이름 사용
	UTexture* DiffuseTexture = Material->GetDiffuseTexture();
	if (DiffuseTexture)
	{
		FString TexturePath = DiffuseTexture->GetFilePath().ToString();
		if (!TexturePath.empty())
		{
			// 파일 이름만 추출 (확장자 제외)
			size_t LastSlash = TexturePath.find_last_of("/\\");
			size_t LastDot = TexturePath.find_last_of(".");

			if (LastSlash != std::string::npos)
			{
				FString FileName = TexturePath.substr(LastSlash + 1);
				if (LastDot != std::string::npos && LastDot > LastSlash)
				{
					FileName = FileName.substr(0, LastDot - LastSlash - 1);
				}
				return FileName + " (Mat)";
			}
		}
	}

	// 3순위: 다른 텍스처들도 시도
	TArray<UTexture*> Textures = {
		Material->GetAmbientTexture(),
		Material->GetSpecularTexture(),
		Material->GetNormalTexture(),
		Material->GetAlphaTexture(),
		Material->GetBumpTexture()
	};

	for (UTexture* Texture : Textures)
	{
		if (Texture)
		{
			FString TexturePath = Texture->GetFilePath().ToString();
			if (!TexturePath.empty())
			{
				size_t LastSlash = TexturePath.find_last_of("/\\");
				size_t LastDot = TexturePath.find_last_of(".");

				if (LastSlash != std::string::npos)
				{
					FString FileName = TexturePath.substr(LastSlash + 1);
					if (LastDot != std::string::npos && LastDot > LastSlash)
					{
						FileName = FileName.substr(0, LastDot - LastSlash - 1);
					}
					return FileName + " (Mat)";
				}
			}
		}
	}

	// 4순위: UUID 기반 이름 사용
	return "Material_" + std::to_string(Material->GetUUID());
}
UTexture* UStaticMeshComponentWidget::GetPreviewTextureForMaterial(UMaterial* Material) const
{
	if (Material == nullptr)
	{
		return nullptr;
	}

	UTexture* PreviewTexture = nullptr;

	PreviewTexture = Material->GetDiffuseTexture();
	if (PreviewTexture != nullptr) { return PreviewTexture; }

	PreviewTexture = Material->GetAmbientTexture();
	if (PreviewTexture != nullptr) { return PreviewTexture; }

	PreviewTexture = Material->GetSpecularTexture();
	if (PreviewTexture != nullptr) { return PreviewTexture; }

	PreviewTexture = Material->GetNormalTexture();
	if (PreviewTexture != nullptr) { return PreviewTexture; }

	PreviewTexture = Material->GetAlphaTexture();
	if (PreviewTexture != nullptr) { return PreviewTexture; }

	PreviewTexture = Material->GetBumpTexture();
	return PreviewTexture;

}