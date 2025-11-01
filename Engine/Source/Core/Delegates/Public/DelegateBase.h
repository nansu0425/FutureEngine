#pragma once
#include "IDelegateInstance.h"

/*-----------------------------------------------------------------------------
    TDelegateBase
 -----------------------------------------------------------------------------*/

class TDelegateBase
{
private:
    friend class TMulticastDelegateBase;
    
protected:
    explicit TDelegateBase() = default;

public:
    ~TDelegateBase()
    {
        Unbind();
    }

    /**
     * @note 언리얼 엔진에는 TDelegateBase를 위한 복사 생성자가 없지만, 편의를 위해 복사 생성자를 정의한다.
     */
    TDelegateBase(const TDelegateBase&) = default;
    TDelegateBase& operator=(const TDelegateBase&) = default;
    
    TDelegateBase(TDelegateBase&& Other) = default;
    TDelegateBase& operator=(TDelegateBase&& Other) = default;

    /**
     * @brief 델리게이트를 언바인드한다.
     */
    inline void Unbind()
    {
        DelegateInstance.reset(); 
    }

    /**
     * @brief UObject 델리게이트일 경우, UObject를 반환한다. 
     */
    inline class UObject* GetUObject() const
    {
        return DelegateInstance ? DelegateInstance->GetUObject() : nullptr;
    }

    /**
     * @brief 델리게이트에 바인딩된 객체가 유효한지 확인한다.
     */
    inline bool IsBound() const
    {
        return DelegateInstance && DelegateInstance->IsSafeToExecute();
    }

    /**
     * @brief 델리게이트에 대한 핸들을 반환한다.
     */
    inline FDelegateHandle GetHandle() const
    {
        return DelegateInstance ? DelegateInstance->GetHandle() : FDelegateHandle{};
    }

protected:
    /**
     * @brief 델리게이트 인스턴스를 반환한다. 사용자가 사용할 용도로 고안된 함수가 아니다.
     */
    inline IDelegateInstance* GetDelegateInstanceProtected()
    {
        return DelegateInstance.get(); 
    }

    inline const IDelegateInstance* GetDelegateInstanceProtected() const
    {
        return DelegateInstance.get(); 
    }

    inline void SetDelegateInstance(IDelegateInstance* InDelegateInstance)
    {
        DelegateInstance.reset(InDelegateInstance);        
    }
    
    inline void SetDelegateInstance(std::shared_ptr<IDelegateInstance> InDelegateInstance)
    {
        DelegateInstance = std::move(InDelegateInstance);        
    }
    
private:
    /**
     * @note 언리얼 엔진에서는 FDelegateAllocation::DelegateAllocator를 사용하지만 편의를 위해 std::shared_ptr를 사용한다.
     */
    std::shared_ptr<IDelegateInstance> DelegateInstance;
};

/*-----------------------------------------------------------------------------
    TMultiDelegateBase
 -----------------------------------------------------------------------------*/

class TMulticastDelegateBase
{
protected:
    using UnicastDelegateType = TDelegateBase;
    
    using InvocationListType = TArray<UnicastDelegateType>;

public:
    /**
     * @note 언리얼 엔진에는 TMultiDelegateBase를 위한 복사 생성자가 없지만, 편의를 위해 복사 생성자를 정의한다.
     */
    TMulticastDelegateBase(const TMulticastDelegateBase& Other) = default;
    TMulticastDelegateBase& operator=(const TMulticastDelegateBase& Other) = default;
    
    TMulticastDelegateBase(TMulticastDelegateBase&& Other) = default;
    TMulticastDelegateBase& operator=(TMulticastDelegateBase&& Other) = default;

    /** 이 델리게이트의 invocation list에 속한 모든 함수들을 제거한다. */
    void Clear()
    {
        InvocationList.clear(); 
    }

    /**
     * @brief 아무 함수라도 이 multi-cast 델리게이트에 바인딩되었는지 확인한다.
     * 
     * @return 아무 함수라도 바인딩 되었으면 true, 아니면 false를 반환한다.
     */
    inline bool IsBound() const
    {
        for (const UnicastDelegateType& DelegateBaseRef : InvocationList)
        {
            if (DelegateBaseRef.GetDelegateInstanceProtected())
            {
                return true; 
            }
        }
        return false;
    }

protected:
    /** 숨겨진 디폴트 생성자 */
    inline TMulticastDelegateBase() = default;

    template <typename DelegateInstanceInterfaceType, typename... ParamTypes>
    void Broadcast(ParamTypes... Params) const
    {
        bool NeedsCompaction = false;

        const InvocationListType& LocalInvocationList = GetInvocationList();

        for (int32 InvocationListIndex = static_cast<int32>(LocalInvocationList.size()) - 1; InvocationListIndex >= 0; --InvocationListIndex)
        {
            const UnicastDelegateType& DelegateBase = LocalInvocationList[InvocationListIndex];

            const IDelegateInstance* DelegateInstanceInterface = DelegateBase.GetDelegateInstanceProtected();
            if (DelegateInstanceInterface == nullptr || !static_cast<const DelegateInstanceInterfaceType*>(DelegateInstanceInterface)->ExecuteIfSafe(Params...))
            {
                NeedsCompaction = true;         
            }
        }

        if (NeedsCompaction)
        {
            const_cast<TMulticastDelegateBase*>(this)->CompactInvocationList();
        }
    }

    /**
     * @brief invocation list에 주어진 델리게이트를 삽입한다. 
     */
    template <typename NewDelegateType>
    inline FDelegateHandle AddDelegateInstance(NewDelegateType&& NewDelegateBaseRef)
    {
        FDelegateHandle Result;
        if (NewDelegateBaseRef.IsBound())
        {
            CompactInvocationList();
            Result = NewDelegateBaseRef.GetHandle();
            InvocationList.emplace_back(std::forward<NewDelegateType>(NewDelegateBaseRef));
        }
        return Result;
    }

    /**
     * @brief multi-cast 델리게이트의 invocation list에서 함수를 삭제한다 (O(N)).
     * 
     * @return 델리게이트가 성공적으로 삭제되면 true
     */
    bool RemoveDelegateInstance(FDelegateHandle Handle)
    {
        for (int32 InvocationListIndex = 0; InvocationListIndex < static_cast<int32>(InvocationList.size()); ++InvocationListIndex)
        {
            UnicastDelegateType& DelegateBase = InvocationList[InvocationListIndex];

            IDelegateInstance* DelegateInstance = DelegateBase.GetDelegateInstanceProtected();
            if (DelegateInstance && DelegateInstance->GetHandle() == Handle)
            {
                DelegateBase.Unbind();
                CompactInvocationList();
                return true;
            }
        }
        return false;
    }

private:
    /**
     * @brief 만료되거나 삭제된 함수들을 invocation list로부터 제거한다.
     */
    void CompactInvocationList()
    {
        int32 OldNumItems = static_cast<int32>(InvocationList.size());

        int32 ValidInvocationListIndex = 0;

        for (int32 InvocationListIndex = 0; InvocationListIndex < static_cast<int32>(InvocationList.size()); ++InvocationListIndex)
        {
            auto& DelegateBaseRef = InvocationList[InvocationListIndex];

            IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
            /** 언리얼 엔진은 IsCompactible()을 사용하지만, 여기서는 IsSafeToExecute()를 사용한다. */
            if (DelegateInstance != nullptr && DelegateInstance->IsSafeToExecute())
            {
                if (ValidInvocationListIndex != InvocationListIndex)
                {
                    InvocationList[ValidInvocationListIndex] = std::move(DelegateBaseRef); 
                }
                ++ValidInvocationListIndex;
            }
        }

        if (ValidInvocationListIndex < OldNumItems)
        {
            InvocationList.erase(InvocationList.begin() + ValidInvocationListIndex);
        }
    }

    inline InvocationListType& GetInvocationList()
    {
        return InvocationList;
    }

    inline const InvocationListType& GetInvocationList() const
    {
        return InvocationList;
    }
    
    /** 호출할 델리게이트 인스턴스들을 저장한다. */
    InvocationListType InvocationList;
};