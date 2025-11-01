#pragma once

#include <functional>

#include "DelegateBase.h"
#include "DelegateInstances.h"
#include "IDelegateInstance.h"

/*-----------------------------------------------------------------------------
    TDelegateRegistration
 -----------------------------------------------------------------------------*/

template <typename FuncType>
class TDelegateRegistration;

template <typename RetValType, typename... ParamTypes>
class TDelegateRegistration<RetValType(ParamTypes...)> : public TDelegateBase
{
protected:
    using FuncType = RetValType(ParamTypes...);
    using DelegateInstanceInterfaceType = IBaseDelegateInstance<FuncType>;

private:
    template <typename>
    friend class TMulticastDelegateRegistration;

protected:
    TDelegateRegistration() = default;
    TDelegateRegistration(TDelegateRegistration&&) = default;
    TDelegateRegistration(const TDelegateRegistration&) = default;
    TDelegateRegistration& operator=(TDelegateRegistration&&) = default;
    TDelegateRegistration& operator=(const TDelegateRegistration&) = default;
    ~TDelegateRegistration() = default;

public:
    /*
     * @brief 전역함수, 정적 멤버함수 포인터에 대한 델리게이트를 바인드한다.
     */
    inline void BindStatic(typename TBaseStaticDelegateInstance<FuncType>::FFuncPtr InFuncPtr)
    {
        SetDelegateInstance(std::make_shared<TBaseStaticDelegateInstance<RetValType(ParamTypes...)>>(InFuncPtr)); 
    }

    /*
     * @brief C++ 람다 델리게이트를 바인드한다.
     * @note 기술적으로 모든 functor 타입에 대해 작동하지만, 람다가 가장 주된 사용처이다.
     */
    template <typename FunctorType>
    inline void BindLambda(FunctorType&& InFunctor)
    {
        SetDelegateInstance(std::make_shared<TBaseFunctorDelegateInstance<FuncType, std::decay_t<FunctorType>>>(std::forward<FunctorType>(InFunctor)));
    }

    /*
     * @brief 약한 참조로 관리되는 C++ 객체의 람다 델리게이트를 바인드한다.
     * @note 기술적으로 모든 functor 타입에 대해 작동하지만, 람다가 가장 주된 사용처이다.
     */
    template <typename FunctorType>
    inline void BindWeakLambda(const UObject* InUserObject, FunctorType&& InFunctor)
    {
        SetDelegateInstance(std::make_shared<TWeakBaseFunctorDelegateInstance<FuncType, std::decay_t<FunctorType>>>(InUserObject, std::forward<FunctorType>(InFunctor)));
    }

    /*
     * @brief raw C++ 포인터 델리게이트를 바인드한다.
     *
     * @note Raw 포인터는 수명이 자동으로 관리되지 않으므로, 함수호출이 안전하지 않을 수 있다.
     *       Execute()를 호출할때는 조심해야한다!
     */
    template <typename UserClass>
    inline void BindRaw(UserClass* InUserObject, typename TMemFuncPtrType<false, UserClass, RetValType(ParamTypes...)>::Type InFunc)
    {
        static_assert(!std::is_const_v<UserClass>, "const 포인터에 대한 델리게이트를 non-const 멤버 함수와 바인딩할 수 없습니다.");
        SetDelegateInstance(std::make_shared<TBaseRawMethodDelegateInstance<false, UserClass, RetValType(ParamTypes...)>>(InUserObject, InFunc));
    }
    
    template <typename UserClass>
    inline void BindRaw(const UserClass* InUserObject, typename TMemFuncPtrType<true, UserClass, RetValType(ParamTypes...)>::Type InFunc)
    {
        SetDelegateInstance(std::make_shared<TBaseRawMethodDelegateInstance<true, UserClass, RetValType(ParamTypes...)>>(const_cast<UserClass*>(InUserObject), InFunc));
    }

    /*
     * @brief shared pointer 기반의 멤버 함수 델리게이트를 바인드한다.
     *
     * @note shared 포인터 델리게이트는 오브젝트에 대한 약한 참조를 저장한다.
     */
    template <typename UserClass>
    inline void BindSP(const std::shared_ptr<UserClass>& InUserObjectRef, typename TMemFuncPtrType<false, UserClass, RetValType(ParamTypes...)>::Type InFunc)
    {
        static_assert(!std::is_const_v<UserClass>, "const 포인터에 대한 델리게이트를 non-const 멤버 함수와 바인딩할 수 없습니다.");
        SetDelegateInstance(std::make_shared<TBaseSPMethodDelegateInstance<false, UserClass, RetValType(ParamTypes...)>>(InUserObjectRef, InFunc));
    }
    
    template <typename UserClass>
    inline void BindSP(const std::shared_ptr<UserClass>& InUserObjectRef, typename TMemFuncPtrType<true, UserClass, RetValType(ParamTypes...)>::Type InFunc)
    {
        SetDelegateInstance(std::make_shared<TBaseSPMethodDelegateInstance<true, UserClass, RetValType(ParamTypes...)>>(InUserObjectRef, InFunc));
    }

    /*
     * @brief UObject 기반 멤버함수 델리게이트를 바인드한다.
     *
     * @note UObject 델리게이트는 오브젝트에 대한 약한 참조를 저장한다.
     */
    template <typename UserClass>
    inline void BindUObject(UserClass* InUserObject, typename TMemFuncPtrType<false, UserClass, RetValType(ParamTypes...)>::Type InFunc)
    {
        SetDelegateInstance(std::make_shared<TBaseUObjectMethodDelegateInstance<false, UserClass, RetValType(ParamTypes...)>>(InUserObject, InFunc));
    }
    
    template <typename UserClass>
    inline void BindUObject(const UserClass* InUserObject, typename TMemFuncPtrType<true, UserClass, RetValType(ParamTypes...)>::Type InFunc)
    {
        SetDelegateInstance(std::make_shared<TBaseUObjectMethodDelegateInstance<true, UserClass, RetValType(ParamTypes...)>>(const_cast<UserClass*>(InUserObject), InFunc));
    }
};

/*-----------------------------------------------------------------------------
    TDelegate
 -----------------------------------------------------------------------------*/

/**
 * @brief 전달된 타입과 같은 타입을 반환한다. 보통 템플릿 인자 추론을 막기위해 사용한다.
 *
 * template <typename T>
 * void Func1(T Val); // Func1(123) 또는 Func1<int>(123)으로 호출될 수 있다.
 *
 * template <typename T>
 * void Func2(typename TIdentity<T>::Type Val); // Func2<int>(123)으로만 호출될 수 있다.
 *
 * C++20의 std::type_identity와 동일하다.
 */
template <typename T>
struct TIdentity
{
    using Type = T;
    using type = T;
};

template <typename T>
using TIdentity_T = typename TIdentity<T>::Type;

template <typename FuncType>
class TDelegate;

template <typename InRetValType, typename... ParamTypes>
class TDelegate<InRetValType(ParamTypes...)> : public TDelegateRegistration<InRetValType(ParamTypes...)>
{
private:
    using Super                         = TDelegateRegistration<InRetValType(ParamTypes...)>;
    using DelegateInstanceInterfaceType = typename Super::DelegateInstanceInterfaceType;
    using FuncType                      = typename Super::FuncType;

public:
    using RegistrationType = Super;

    /** 반환 값 타입을 위한 타입 정의 */
    using RetValType = InRetValType;
    using TFuncType  = InRetValType(ParamTypes...);

    /** 델리게이트들에 대한 함수 포인터 타입을 얻기 위한 헬퍼 typedef들 */
    using TFuncPtr        = RetValType(*)(ParamTypes...);
    template<typename UserClass>
    using TMethodPtr      = typename TMemFuncPtrType<false, UserClass, RetValType(ParamTypes...)>::Type;
    template<typename UserClass>
    using TConstMethodPtr = typename TMemFuncPtrType<true, UserClass, RetValType(ParamTypes...)>::Type;

    TDelegate() = default;
    
    TDelegate(const TDelegate& Other) = default;
    TDelegate& operator=(const TDelegate& Other) = default;
    
    TDelegate(TDelegate&& Other) = default;
    TDelegate& operator=(TDelegate&& Other) = default;
    
    ~TDelegate() = default;

    /**
     * @brief Raw C++ 전역 함수에 대한 델리게이트를 생성한다.
     */
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateStatic(typename TIdentity<RetValType (*)(ParamTypes...)>::Type InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindStatic(InFunc);
        return Result;
    }

    /**
     * @brief C++ 람다 델리게이트를 생성한다.
     * @note 기술적으로 모든 functor 타입에 작동하지만, 람다가 가장 주된 사용처이다. 
     */
    template <typename FunctorType>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateLambda(FunctorType&& InFunctor)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindLambda(std::forward<FunctorType>(InFunctor));
        return Result;
    }

    template <typename FunctorType>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateWeakLambda(const UObject* InUserObject, FunctorType&& InFunctor)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindWeakLambda(InUserObject, InFunctor);
        return Result;
    }

    /**
     * @brief Raw C++ 멤버 함수 포인터에 대한 델리게이트를 생성한다.
     * @note Raw 포인터는 수명이 자동으로 관리되지 않으므로, 함수호출이 안전하지 않을 수 있다.
     *       Execute()를 호출할때는 조심해야한다!
     */
    template <typename UserClass>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateRaw(UserClass* InUserObject, TMethodPtr<UserClass> InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindRaw(InUserObject, InFunc);
        return Result;
    }

    template <typename UserClass>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateRaw(const UserClass* InUserObject, TConstMethodPtr<UserClass> InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindRaw(InUserObject, InFunc);
        return Result;
    }

    /*
     * @brief shared pointer 기반의 멤버 함수 델리게이트를 생성한다.
     *
     * @note shared 포인터 델리게이트는 오브젝트에 대한 약한 참조를 저장한다.
     *       ExecuteIfBound()를 사용해서 호출할수 있다.
     */
    template <typename UserClass>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateSP(const std::shared_ptr<UserClass>& InUserObjectRef, TMethodPtr<UserClass> InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindSP(InUserObjectRef, InFunc);
        return Result;
    }

    template <typename UserClass>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateSP(const std::shared_ptr<UserClass>& InUserObjectRef, TConstMethodPtr<UserClass> InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindSP(InUserObjectRef, InFunc);
        return Result;
    }

    /*
     * @brief UObject 기반 멤버함수 델리게이트를 생성한다.
     *
     * @note UObject 델리게이트는 오브젝트에 대한 약한 참조를 저장한다.
     *       ExecuteIfBound()를 사용해서 호출할 수 있다.
     */
    template <typename UserClass>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateUObject(UserClass* InUserObject, TMethodPtr<UserClass> InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindUObject(InUserObject, InFunc);
        return Result;
    }

    template <typename UserClass>
    [[nodiscard]] inline static TDelegate<RetValType(ParamTypes...)> CreateUObject(const UserClass* InUserObject, TConstMethodPtr<UserClass> InFunc)
    {
        TDelegate<RetValType(ParamTypes...)> Result;
        Result.BindUObject(InUserObject, InFunc);
        return Result;
    }

public:
    /**
     * @brief 델리게이트를 실행한다.
     *
     * 함수 포인터가 유효하지 않으면 오류가 발생한다. 이 메소드를 호출하기 전에 IsBound()를 통해서 확인하거나,
     * ExecuteIfBound()를 사용하라.
     */
    RetValType Execute(ParamTypes... Params) const
    {
        const DelegateInstanceInterfaceType* LocalDelegateInstance = Super::GetDelegateInstanceProtected();

        assert(LocalDelegateInstance != nullptr);

        return LocalDelegateInstance->Execute(std::forward<ParamTypes>(Params)...);
    }

    /**
     * @brief 함수 포인터가 유효할 경우에만 델리게이트를 실행한다.
     *
     * @return 함수가 실행되었을 경우에만 true를 반환한다.
     *
     * @note 현재 반환값이 없는 델리게이트만 ExecuteIfBound()를 지원한다.
     * @note 'DummyRetValType'은 SFINAE를 올바르게 적용하기 위한 템플릿 트릭이다.
     *       std::enable_if_t의 조건이 클래스 템플릿 파라미터('RetValType')에 직접 의존하면
     *       SFINAE(치환 실패)가 아닌 즉각적인 컴파일 오류가 발생한다.
     *       조건을 이 함수 템플릿 파라미터('DummyRetValType')에 의존시켜야
     *       'RetValType'이 void가 아닐 때 이 함수가 오버로딩 후보에서 올바르게 제외된다.
     */
    template<
        typename DummyRetValType = RetValType,
        std::enable_if_t<std::is_void<DummyRetValType>::value>* = nullptr
    >
    inline bool ExecuteIfBound(ParamTypes... Params) const
    {
        if (const DelegateInstanceInterfaceType* Ptr = Super::GetDelegateInstanceProtected())
        {
            return Ptr->ExecuteIfSafe(std::forward<ParamTypes>(Params)...);
        }

        return false;
    }
};

/*-----------------------------------------------------------------------------
    TMulticastDelegateRegistration
 -----------------------------------------------------------------------------*/

template <typename... ParamTypes>
class TMulticastDelegateRegistration<void(ParamTypes...)> : public TMulticastDelegateBase
{
protected:
    using Super                         = typename TMulticastDelegateBase;
    using DelegateInstanceInterfaceType = IBaseDelegateInstance<void (ParamTypes...)>;

private:
    using InvocationListType = typename Super::InvocationListType;

public:
    using FDelegate = TDelegate<void(ParamTypes...)>;

protected:
    TMulticastDelegateRegistration() = default;
    TMulticastDelegateRegistration(const TMulticastDelegateRegistration&) = default;
    TMulticastDelegateRegistration(TMulticastDelegateRegistration&&) = default;
    TMulticastDelegateRegistration& operator=(const TMulticastDelegateRegistration&) = default;
    TMulticastDelegateRegistration& operator=(TMulticastDelegateRegistration&&) = default;
    ~TMulticastDelegateRegistration() = default;
    
public:
    /**
     * @brief multi-cast 델리게이트의 invocation list에 델리게이트를 삽입한다.
     */
    FDelegateHandle Add(FDelegate&& InNewDelegate)
    {
        return Super::AddDelegateInstance(std::move(InNewDelegate)); 
    }
    
    /**
     * @brief multi-cast 델리게이트의 invocation list에 델리게이트를 삽입한다.
     */
    FDelegateHandle Add(const FDelegate& InNewDelegate)
    {
        return Super::AddDelegateInstance(InNewDelegate); 
    }

    /**
     * @brief 전역함수에 대한 Raw C++ 포인터를 삽입한다.
     * 
     * @param InFunc 함수 포인터
     */
    inline FDelegateHandle AddStatic(typename TBaseStaticDelegateInstance<void (ParamTypes...)>::FFuncPtr InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateStatic(InFunc));
    }

    /**
     * @brief C++ 람다 델리게이트를 삽입한다.
     * 
     * @note 기술적으로 모든 functor 타입에 작동하지만, 람다가 가장 주된 사용처이다.
     *
     * @param InFunctor Functor (e.g., 람다)
     */
    template <typename FunctorType>
    inline FDelegateHandle AddLambda(FunctorType&& InFunctor)
    {
        return Super::AddDelegateInstance(FDelegate::CreateLambda(std::forward<FunctorType>(InFunctor)));
    }

    template <typename UserClass, typename FunctorType>
    inline FDelegateHandle AddWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor)
    {
        return Super::AddDelegateInstance(FDelegate::CreateWeakLambda(InUserObject, std::forward<FunctorType>(InFunctor))); 
    }

    /**
     * @brief Raw C++ 멤버 함수 포인터를 삽입한다.
     * 
     * @note Raw 포인터는 수명이 자동으로 관리되지 않으므로, 함수호출이 안전하지 않을 수 있다.
     *       Execute()를 호출할때는 조심해야한다!
     */
    template <typename UserClass>
    inline FDelegateHandle AddRaw(UserClass* InUserObject, typename FDelegate::template TMethodPtr<UserClass> InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateRaw(InUserObject, InFunc));
    }

    template <typename UserClass>
    inline FDelegateHandle AddRaw(const UserClass* InUserObject, typename FDelegate::template TConstMethodPtr<UserClass> InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateRaw(InUserObject, InFunc));
    }

    /*
     * @brief shared pointer 기반의 멤버 함수 델리게이트를 삽입한다.
     *
     * @note shared 포인터 델리게이트는 오브젝트에 대한 약한 참조를 저장한다.
     *       ExecuteIfBound()를 사용해서 호출할수 있다.
     */
    template <typename UserClass>
    inline FDelegateHandle AddSP(const std::shared_ptr<UserClass>& InUserObjectRef, typename FDelegate::template TMethodPtr<UserClass> InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateSP(InUserObjectRef, InFunc));
    }

    template <typename UserClass>
    inline FDelegateHandle AddSP(const std::shared_ptr<UserClass>& InUserObjectRef, typename FDelegate::template TConstMethodPtr<UserClass> InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateSP(InUserObjectRef, InFunc));
    }

    /*
     * @brief UObject 기반 멤버함수 델리게이트를 삽입한다.
     *
     * @note UObject 델리게이트는 오브젝트에 대한 약한 참조를 저장한다.
     *       ExecuteIfBound()를 사용해서 호출할 수 있다.
     */
    template <typename UserClass>
    inline FDelegateHandle AddUObject(UserClass* InUserObject, typename FDelegate::template TMethodPtr<UserClass> InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateUObject(InUserObject, InFunc));
    }

    template <typename UserClass>
    inline FDelegateHandle AddUObject(const UserClass* InUserObject, typename FDelegate::template TConstMethodPtr<UserClass> InFunc)
    {
        return Super::AddDelegateInstance(FDelegate::CreateUObject(InUserObject, InFunc));
    }

public:
    /**
     * @brief multi-cast 델리게이트의 invocation list에서 델리게이트 인스턴스를 제거한다 (O(N)).
     * 
     * @param Handle 삭제할 델리게이트 인스턴스의 핸들
     * @return 델리게이트가 성공적으로 삭제되면 true
     */
    bool Remove(FDelegateHandle Handle)
    {
        bool bResult = false;
        if (Handle.IsValid())
        {
            bResult = RemoveDelegateInstance(Handle);
        }
        return bResult;
    }

    /** TMulticastDelegateRegistration을 이용한 Broadcast()의 호출은 허용되지 않는다. */
    void Broadcast(ParamTypes... Params) = delete;
};

template <typename DelegateSignature>
class TMulticastDelegate
{
    static_assert(sizeof(DelegateSignature) == 0, "델리게이트 템플릿 파라미터는 함수 시그니쳐여야 합니다.");
};

template <typename RetValType, typename... ParamTypes>
class TMulticastDelegate<RetValType(ParamTypes...)>
{
    static_assert(sizeof(RetValType) == 0, "multi-cast 델리게이트의 반환 타입은 void여야 합니다.");
};

template <typename... ParamTypes>
class TMulticastDelegate<void(ParamTypes...)> : public TMulticastDelegateRegistration<void(ParamTypes...)>
{
private:
    using Super                         = TMulticastDelegateRegistration<void(ParamTypes...)>;
    using DelegateInstanceInterfaceType = typename Super::DelegateInstanceInterfaceType;

public:
    using RegistrationType = Super;

    TMulticastDelegate() = default;
    TMulticastDelegate(const TMulticastDelegate&) = default;
    TMulticastDelegate(TMulticastDelegate&&) = default;
    TMulticastDelegate& operator=(const TMulticastDelegate&) = default;
    TMulticastDelegate& operator=(TMulticastDelegate&&) = default;
    ~TMulticastDelegate() = default;

    void Broadcast(ParamTypes... Params) const
    {
        Super::Super::template Broadcast<typename Super::DelegateInstanceInterfaceType, ParamTypes...>(Params...);
    }
};

/*-----------------------------------------------------------------------------
    DELEGATE PARAMETER COMBINATIONS (DelegateCombinations.h)
 -----------------------------------------------------------------------------*/

/**
 * @brief 한 번에 하나의 함수에 바인딩되는 델리게이트를 선언한다.
 *
 * @note 마지막 매개변수는 가변인자이며, 델리게이트 클래스의 'template args'로 사용된다 (__VA_ARGS__).
 */
#define FUNC_DECLARE_DELEGATE(DelegateName, ReturnType, ...) \
    typedef TDelegate<ReturnType<__VA_ARGS__)> DelegateName;

/** @brief 다수의 함수와 동시에 바인딩될 수 있는 multi-cast 델리게이트를 선언한다. */
#define FUNC_DECLARE_MULTICAST_DELEGATE(MulticastDelegateName, ReturnType, ...) \
    typedef TMulticastDelegate<ReturnType(__VA_ARGS__)> MulticastDelegateName;

/**
 * @brief 한 번에 하나의 함수에 바인딩되는 델리게이트를 선언한다.
 * @note 언리얼엔진과 달리 편의를 위해 _NParam 버전 대신 가변인자 버전을 제공한다.
 */
#define DECLARE_DELEGATE(DelegateName, ReturnType, ...) FUNC_DECLARE_DELEGATE(DelegateName, ReturnType, ##__VA_ARGS__)

/**
 * @brief 다수의 함수와 동시에 바인딩될 수 있는 multi-cast 델리게이트를 선언한다.
 * @note 언리얼엔진과 달리 편의를 위해 _NParam 버전 대신 가변인자 버전을 제공한다.
 */
#define DECLARE_MULTICAST_DELEGATE(DelegateName, ...) FUNC_DECLARE_MULTICAST_DELEGATE(DelegateName, void, ##__VA_ARGS__)

