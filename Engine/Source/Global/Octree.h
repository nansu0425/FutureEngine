#pragma once

#include "Physics/Public/AABB.h"

class UPrimitiveComponent;

constexpr int MAX_PRIMITIVES = 16; 
constexpr int MAX_DEPTH = 5;      

class FOctree
{
public:
	FOctree();
	FOctree(const FVector& InPosition, float InSize, int InDepth);
	FOctree(const FAABB& InBoundingBox, int InDepth);
	~FOctree();

	bool Insert(UPrimitiveComponent* InPrimitive);
	bool Remove(UPrimitiveComponent* InPrimitive);
	void Clear();

	void DeepCopy(FOctree* OutOctree) const;

	void GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const;
	TArray<UPrimitiveComponent*> FindNearestPrimitives(const FVector& FindPos, uint32 MaxPrimitiveCount);

	const FAABB& GetBoundingVolume() const { return BoundingVolume; }
	void SetBoundingVolume(const FAABB& InAABB) { BoundingVolume = InAABB; }
	bool IsLeafNode() const { return IsLeaf(); }
	const TArray<UPrimitiveComponent*>& GetPrimitives() const { return Primitives; }
	TArray<FOctree*>& GetChildren() { return Children; }
	const TArray<FOctree*>& GetChildren() const { return Children; }

private:
	bool IsLeaf() const { return Children[0] == nullptr; }
	void Subdivide(UPrimitiveComponent* InPrimitive);
	void TryMerge();

	FAABB BoundingVolume;
	int Depth;                       
	TArray<UPrimitiveComponent*> Primitives;
	TArray<FOctree*> Children;
};

using FNodeQueue = std::priority_queue<
	std::pair<float, FOctree*>,
	std::vector<std::pair<float, FOctree*>>,
	std::greater<std::pair<float, FOctree*>>
>;
