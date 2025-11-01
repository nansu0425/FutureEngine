#include "pch.h"
#include "Component/Public/PrimitiveComponent.h"

#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_ABSTRACT_CLASS(UPrimitiveComponent, USceneComponent)

UPrimitiveComponent::UPrimitiveComponent()
{
	bCanEverTick = true;
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);
}
void UPrimitiveComponent::OnSelected()
{
	SetColor({ 1.f, 0.8f, 0.2f, 0.4f });
}

void UPrimitiveComponent::OnDeselected()
{
	SetColor({ 0.f, 0.f, 0.f, 0.f });
}

void USceneComponent::SetUniformScale(bool bIsUniform)
{
	bIsUniformScale = bIsUniform;
}

bool USceneComponent::IsUniformScale() const
{
	return bIsUniformScale;
}

const TArray<FNormalVertex>* UPrimitiveComponent::GetVerticesData() const
{
	return Vertices;
}

const TArray<uint32>* UPrimitiveComponent::GetIndicesData() const
{
	return Indices;
}

ID3D11Buffer* UPrimitiveComponent::GetVertexBuffer() const
{
	return VertexBuffer;
}

ID3D11Buffer* UPrimitiveComponent::GetIndexBuffer() const
{
	return IndexBuffer;
}

uint32 UPrimitiveComponent::GetNumVertices() const
{
	return NumVertices;
}

uint32 UPrimitiveComponent::GetNumIndices() const
{
	return NumIndices;
}

void UPrimitiveComponent::SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	Topology = InTopology;
}

D3D11_PRIMITIVE_TOPOLOGY UPrimitiveComponent::GetTopology() const
{
	return Topology;
}

const IBoundingVolume* UPrimitiveComponent::GetBoundingVolume()
{
	BoundingVolume->Update(GetWorldTransformMatrix());
	return BoundingVolume;
}

void UPrimitiveComponent::GetWorldAABB(FVector& OutMin, FVector& OutMax)
{
	if (!BoundingVolume)
	{
		OutMin = FVector(); OutMax = FVector();
		return;
	}

	if (bIsAABBCacheDirty)
	{
		if (BoundingVolume->GetType() == EBoundingVolumeType::AABB)
		{
			const FAABB* LocalAABB = static_cast<const FAABB*>(BoundingVolume);
			FVector LocalCorners[8] =
			{
				FVector(LocalAABB->Min.X, LocalAABB->Min.Y, LocalAABB->Min.Z), FVector(LocalAABB->Max.X, LocalAABB->Min.Y, LocalAABB->Min.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Max.Y, LocalAABB->Min.Z), FVector(LocalAABB->Max.X, LocalAABB->Max.Y, LocalAABB->Min.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Min.Y, LocalAABB->Max.Z), FVector(LocalAABB->Max.X, LocalAABB->Min.Y, LocalAABB->Max.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Max.Y, LocalAABB->Max.Z), FVector(LocalAABB->Max.X, LocalAABB->Max.Y, LocalAABB->Max.Z)
			};

			const FMatrix& WorldTransform = GetWorldTransformMatrix();
			FVector WorldMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
			FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (int32 Idx = 0; Idx < 8; Idx++)
			{
				FVector4 WorldCorner = FVector4(LocalCorners[Idx].X, LocalCorners[Idx].Y, LocalCorners[Idx].Z, 1.0f) * WorldTransform;
				WorldMin.X = min(WorldMin.X, WorldCorner.X);
				WorldMin.Y = min(WorldMin.Y, WorldCorner.Y);
				WorldMin.Z = min(WorldMin.Z, WorldCorner.Z);
				WorldMax.X = max(WorldMax.X, WorldCorner.X);
				WorldMax.Y = max(WorldMax.Y, WorldCorner.Y);
				WorldMax.Z = max(WorldMax.Z, WorldCorner.Z);
			}

			CachedWorldMin = WorldMin;
			CachedWorldMax = WorldMax;
		}
		else if (BoundingVolume->GetType() == EBoundingVolumeType::OBB ||
			BoundingVolume->GetType() == EBoundingVolumeType::SpotLight)
		{
			const FOBB* OBB = static_cast<const FOBB*>(GetBoundingVolume());
			FAABB AABB = OBB->ToWorldAABB();

			CachedWorldMin = AABB.Min;
			CachedWorldMax = AABB.Max;
       }

		bIsAABBCacheDirty = false;
	}

	// 캐시된 값 반환
	OutMin = CachedWorldMin;
	OutMax = CachedWorldMax;
}

void UPrimitiveComponent::MarkAsDirty()
{
	bIsAABBCacheDirty = true;
	Super::MarkAsDirty();
}


UObject* UPrimitiveComponent::Duplicate()
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Super::Duplicate());
	
	PrimitiveComponent->Color = Color;
	PrimitiveComponent->Topology = Topology;
	PrimitiveComponent->RenderState = RenderState;
	PrimitiveComponent->bVisible = bVisible;
	PrimitiveComponent->bReceivesDecals = bReceivesDecals;

	PrimitiveComponent->Vertices = Vertices;
	PrimitiveComponent->Indices = Indices;
	PrimitiveComponent->VertexBuffer = VertexBuffer;
	PrimitiveComponent->IndexBuffer = IndexBuffer;
	PrimitiveComponent->NumVertices = NumVertices;
	PrimitiveComponent->NumIndices = NumIndices;

	if (!bOwnsBoundingVolume)
	{
		PrimitiveComponent->BoundingVolume = BoundingVolume;
	}
	
	return PrimitiveComponent;
}

void UPrimitiveComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);

}
void UPrimitiveComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FString VisibleString;
		FJsonSerializer::ReadString(InOutHandle, "bVisible", VisibleString, "true");
		SetVisibility(VisibleString == "true");
	}
	else
	{
		InOutHandle["bVisible"] = bVisible ? "true" : "false";
	}

}

// Collision & Overlap Implementation

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* Other) const
{
	if (!Other)
		return false;

	for (const FOverlapInfo& Info : OverlapInfos)
	{
		if (Info.OverlappingComponent == Other)
			return true;
	}

	return false;
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
	if (!Other)
		return false;

	for (const FOverlapInfo& Info : OverlapInfos)
	{
		if (Info.OverlappingComponent && Info.OverlappingComponent->GetOwner() == Other)
			return true;
	}

	return false;
}

void UPrimitiveComponent::AddOverlapInfo(const FOverlapInfo& Info)
{
	// 중복 체크
	for (const FOverlapInfo& ExistingInfo : OverlapInfos)
	{
		if (ExistingInfo == Info)
			return;
	}

	OverlapInfos.push_back(Info);
}

void UPrimitiveComponent::RemoveOverlapInfo(const UPrimitiveComponent* Component)
{
	for (auto It = OverlapInfos.begin(); It != OverlapInfos.end(); ++It)
	{
		if (It->OverlappingComponent == Component)
		{
			OverlapInfos.erase(It);
			return;
		}
	}
}

bool UPrimitiveComponent::CheckOverlapWith(const UPrimitiveComponent* Other) const
{
	// 기본 구현: 항상 false 반환
	// Shape Component들이 오버라이드하여 실제 충돌 체크 수행
	return false;
}

void UPrimitiveComponent::UpdateOverlaps(const TArray<UPrimitiveComponent*>& AllComponents)
{
	if (!bGenerateOverlapEvents)
		return;

	TArray<FOverlapInfo> NewOverlapInfos;

	// 모든 컴포넌트와 충돌 체크
	for (UPrimitiveComponent* Other : AllComponents)
	{
		if (Other == this || !Other->bGenerateOverlapEvents)
			continue;

		if (CheckOverlapWith(Other))
		{
			NewOverlapInfos.push_back(FOverlapInfo(Other));
		}
	}

	// 새로 겹친 것 확인 (BeginOverlap)
	for (const FOverlapInfo& NewInfo : NewOverlapInfos)
	{
		auto It = std::find(OverlapInfos.begin(), OverlapInfos.end(), NewInfo);
		if (It == OverlapInfos.end())
		{
			// 새로 겹침 - 로그 출력
			AActor* MyOwner = GetOwner();
			AActor* OtherOwner = NewInfo.OverlappingComponent->GetOwner();
			UE_LOG_SUCCESS("BeginOverlap: [%s]%s <-> [%s]%s",
				MyOwner ? MyOwner->GetName().ToString().c_str() : "None",
				GetName().ToString().c_str(),
				OtherOwner ? OtherOwner->GetName().ToString().c_str() : "None",
				NewInfo.OverlappingComponent->GetName().ToString().c_str());
		}
	}

	// 분리된 것 확인 (EndOverlap)
	for (const FOverlapInfo& OldInfo : OverlapInfos)
	{
		auto It = std::find(NewOverlapInfos.begin(), NewOverlapInfos.end(), OldInfo);
		if (It == NewOverlapInfos.end())
		{
			// 분리됨 - 로그 출력
			AActor* MyOwner = GetOwner();
			AActor* OtherOwner = OldInfo.OverlappingComponent->GetOwner();
			UE_LOG_WARNING("EndOverlap: [%s]%s <-> [%s]%s",
				MyOwner ? MyOwner->GetName().ToString().c_str() : "None",
				GetName().ToString().c_str(),
				OtherOwner ? OtherOwner->GetName().ToString().c_str() : "None",
				OldInfo.OverlappingComponent->GetName().ToString().c_str());
		}
	}

	// 새로운 Overlap 정보로 업데이트
	OverlapInfos = NewOverlapInfos;
}