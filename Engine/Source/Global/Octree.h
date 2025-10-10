#pragma once

#include "Physics/Public/AABB.h"

class UPrimitiveComponent;

constexpr int MAX_PRIMITIVES = 32; 
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

	/**
	 * OutOctree로 현재 트리의 내용을 깊은 복사합니다.
	 * - 최상위 노드는 새로 생성하지 않습니다. (new 사용 없음)
	 * - 자식 노드는 필요 시 동적 할당하여 구조를 재귀적으로 동일하게 만듭니다.
	 * - Primitive 포인터들은 참조만 복사됩니다. (shallow copy)
	 *
	 * @param OutOctree 복사 대상 Octree (nullptr가 아니어야 함)
	 *
	 * @code
	 * FOctree Source(BoundingBox, 0);
	 * // ... Source에 여러 프리미티브 삽입 ...
	 * FOctree Dest(BoundingBox, 0);
	 * Source.DeepCopy(&Dest);
	 * @endcode
	 */
	void DeepCopy(FOctree* OutOctree) const;

	void GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const;
	TArray<UPrimitiveComponent*> FindNearestPrimitives(const FVector& FindPos, uint32 MaxPrimitiveCount);

	const FAABB& GetBoundingBox() const { return BoundingBox; }
	void SetBoundingBox(const FAABB& InAABB) { BoundingBox = InAABB; }
	bool IsLeafNode() const { return IsLeaf(); }
	const TArray<UPrimitiveComponent*>& GetPrimitives() const { return Primitives; }
	TArray<FOctree*>& GetChildren() { return Children; }

private:
	bool IsLeaf() const { return Children[0] == nullptr; }
	void Subdivide(UPrimitiveComponent* InPrimitive);
	void TryMerge();

	FAABB BoundingBox;
	int Depth;                       
	TArray<UPrimitiveComponent*> Primitives;
	TArray<FOctree*> Children;
};

using FNodeQueue = std::priority_queue<
	std::pair<float, FOctree*>,
	std::vector<std::pair<float, FOctree*>>,
	std::greater<std::pair<float, FOctree*>>
>;
