#pragma once

template<typename TActor>
class TActorPool
{
public:
    static_assert(std::is_base_of_v<AActor, TActor>);

    using FPoolCreateCallback  = std::function<TActor*()>;
    using FPoolDestroyCallback = std::function<void(TActor*)>;
    using FPoolGetCallback     = std::function<void(TActor*)>;
    using FPoolReturnCallback  = std::function<void(TActor*)>;

    TActorPool()
        : bIsInitialized(false)
    {
    }

    TActorPool(const TActorPool& Other) = delete;
    TActorPool(TActorPool&& Other) = delete;
    TActorPool& operator=(const TActorPool& Other) = delete;
    TActorPool& operator=(TActorPool&& Other) = delete;

    ~TActorPool()
    {
        Shutdown();
    }

    /**
     * @brief 풀을 생성하고 InitialSize만큼의 액터를 미리 생성한다.
     * 
     * @param InPoolCreateCallback 액터 생성용 함수
     * @param InPoolDestroyCallback 액터 소멸용 함수 (액터에 대한 소유권을 풀이 가지지 않을 경우, 아무것도 하지 않는 함수를 전달해야 한다)
     * @param InPoolGetCallback Get() 시 사용할 디폴트 함수
     * @param InPoolReturnCallback Return() 시 사용할 함수
     * @param InitialSize 사전에 생성할 액터 수
     */
    void Initialize(
        FPoolCreateCallback  InPoolCreateCallback,
        FPoolDestroyCallback InPoolDestroyCallback,
        FPoolGetCallback     InPoolGetCallback,
        FPoolReturnCallback  InPoolReturnCallback,
        uint32               InitialSize = INITIAL_SIZE
    )
    {
        if (bIsInitialized)
        {
            return;
        }

        OnCreateCallback = std::move(InPoolCreateCallback);
        OnDestroyCallback = std::move(InPoolDestroyCallback);
        OnDefaultGetCallback = std::move(InPoolGetCallback);
        OnReturnCallback = std::move(InPoolReturnCallback);

        InactivePool.reserve(InitialSize);

        for (uint32 i = 0; i < InitialSize; ++i)
        {
            TActor* Actor = OnCreateCallback();
            if (OnReturnCallback)
            {
                OnReturnCallback(Actor);
            }
            InactivePool.push_back(Actor);
        }
        bIsInitialized = true;
    }

    /**
     * @brief 풀에서 액터를 가져온다. 디폴트 콜백 함수를 사용한다. 
     */
    TActor* Get()
    {
        TActor* Actor = GetInternal();
        if (Actor && OnDefaultGetCallback)
        {
            OnDefaultGetCallback(Actor);
        }
        return Actor;
    }

    /**
     * @brief 풀에서 액터를 가져온다. 
     */
    TActor* Get(FPoolGetCallback OnGetCallback)
    {
        TActor* Actor = GetInternal();
        if (Actor && OnGetCallback)
        {
            OnGetCallback(Actor);
        }
        return Actor;
    }

    /**
     * @brief 사용이 끝난 액터를 풀에 반환한다. 
     */
    void Return(TActor* InActor)
    {
        assert(bIsInitialized);
        
        if (InActor == nullptr)
        {
            return;
        }

        if (OnReturnCallback)
        {
            OnDefaultReturnCallback(InActor);
        }
        InactivePool.push_back(InActor);
    }

    /**
     * @brief 풀을 종료하고 모든 리소스를 해제한다(소유권의 관리는 콜백함수가 담당한다).
     */
    void Shutdown()
    {
        if (!bIsInitialized)
        {
            return;
        }

        if (OnDestroyCallback)
        {
            for (TActor* Actor : InactivePool)
            {
                OnDestroyCallback(Actor);
            }
        }

        InactivePool.clear();

        OnCreateCallback = nullptr;
        OnDestroyCallback = nullptr;
        OnDefaultGetCallback = nullptr;
        OnReturnCallback = nullptr;
        bIsInitialized = false;
    }

private:
    TActor* GetInternal()
    {
        assert(!bIsInitialized);

        if (!bIsInitialized)
        {
            return nullptr;
        }

        TActor* Actor = nullptr;
        if (InactivePool.size() > 0)
        {
            Actor = InactivePool.back();
            InactivePool.pop_back();
        }
        else
        {
            Actor = OnCreateCallback();
        }
        return Actor;
    }
    
    static constexpr uint32 INITIAL_SIZE = 32;

    FPoolCreateCallback  OnCreateCallback;
    FPoolDestroyCallback OnDestroyCallback;
    FPoolGetCallback     OnDefaultGetCallback;
    FPoolReturnCallback  OnReturnCallback;

    TArray<TActor*> InactivePool;

    bool bIsInitialized;
};
