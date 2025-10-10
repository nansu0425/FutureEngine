#pragma once
#include "Component/Public/ActorComponent.h"

namespace json { class JSON; }
using JSON = json::JSON;

UCLASS()
class USceneComponent : public UActorComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USceneComponent, UActorComponent)

public:
	USceneComponent();

	void BeginPlay() override;
	void TickComponent() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	USceneComponent* GetParentComponent() const { return ParentAttachment; }
	void SetParentAttachment(USceneComponent* SceneComponent);
	void AddChild(USceneComponent* Child) { Children.push_back(Child); }
	void RemoveChild(USceneComponent* ChildDeleted);
	
	virtual void MarkAsDirty();

	void SetRelativeLocation(const FVector& Location);
	void SetRelativeRotation(const FVector& Rotation);
	void SetRelativeScale3D(const FVector& Scale);
	void SetUniformScale(bool bIsUniform);

	bool IsUniformScale() const;

	USceneComponent* GetParentAttachment() { return ParentAttachment; }
	TArray<USceneComponent*> GetChildren() { return Children; }

	const FVector& GetRelativeLocation() const { return RelativeLocation; }
	const FVector& GetRelativeRotation() const { return RelativeRotation; }
	const FVector& GetRelativeScale3D() const { return RelativeScale3D; }

	const FMatrix& GetWorldTransformMatrix() const;
	const FMatrix& GetWorldTransformMatrixInverse() const;

	FVector GetWorldLocation() const;
    FVector GetWorldRotation() const;
    FVector GetWorldScale3D() const;

    void SetWorldLocation(const FVector& NewLocation);
    void SetWorldRotation(const FVector& NewRotation);
    void SetWorldScale3D(const FVector& NewScale);

private:
	mutable bool bIsTransformDirty = true;
	mutable bool bIsTransformDirtyInverse = true;
	mutable FMatrix WorldTransformMatrix;
	mutable FMatrix WorldTransformMatrixInverse;

	USceneComponent* ParentAttachment = nullptr;
	TArray<USceneComponent*> Children;
	FVector RelativeLocation = FVector{ 0,0,0.f };
	FVector RelativeRotation = FVector{ 0,0,0.f };
	FVector RelativeScale3D = FVector{ 0.3f,0.3f,0.3f };
	bool bIsUniformScale = false;

public:
	float InactivityTimer = 0.0f;
	float InactivityThreshold = 5.0f;

public:
	virtual UObject* Duplicate() override;

protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;
};