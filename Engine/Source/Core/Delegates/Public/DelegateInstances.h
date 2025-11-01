#pragma once

#include "IDelegateInstance.h"
#include "Core/Public/WeakObjectPtr.h"

/*-----------------------------------------------------------------------------
    TBaseStaticDelegateInstance
 -----------------------------------------------------------------------------*/

template <typename FuncType>
class TBaseStaticDelegateInstance;

/**
 * @brief 전역 함수 또는 static 멤버 함수에 대한 바인딩
 */
template <typename RetValType, typename... ParamTypes>
class TBaseStaticDelegateInstance<RetValType(ParamTypes...)> : public IBaseDelegateInstance<RetValType(ParamTypes...)>
{
private:
    using Super = IBaseDelegateInstance<RetValType(ParamTypes...)>;
    
public:
    using FFuncPtr = RetValType(*)(ParamTypes...);

    explicit TBaseStaticDelegateInstance(FFuncPtr InStaticFuncPtr)
        : StaticFuncPtr(InStaticFuncPtr)
        , Handle(FDelegateHandle::GenerateNewHandle)
    {
        assert(InStaticFuncPtr != nullptr);
    }
    
    /*-----------------------------------------------------------------------------
        IDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    UObject* GetUObject() const final
    {
        return nullptr;
    }

    bool IsSafeToExecute() const final
    {
        /** 정적 함수는 항상 안전하게 실행가능 */
        return true;
    }

    FDelegateHandle GetHandle() const final
    {
        return Handle;
    }

    /*-----------------------------------------------------------------------------
        IBaseDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    RetValType Execute(ParamTypes... Params) const final
    {
        assert(StaticFuncPtr != nullptr);
        
        return StaticFuncPtr(std::forward<ParamTypes>(Params)...);
    }

    bool ExecuteIfSafe(ParamTypes... Params) const final
    {
        assert(StaticFuncPtr != nullptr);
        
        (void)StaticFuncPtr(std::forward<ParamTypes>(Params)...);

        return true;
    }

private:
    FFuncPtr StaticFuncPtr;
    
    FDelegateHandle Handle;
};

/*-----------------------------------------------------------------------------
    TBaseRawMethodDelegateInstance
 -----------------------------------------------------------------------------*/

template <bool bConst, class UserClass, typename FuncType>
class TBaseRawMethodDelegateInstance;

/**
 * @brief Raw C++ 객체의 멤버 함수에 대한 바인딩 (Unsafe)
 */
template <bool bConst, class UserClass, typename RetValType, typename... ParamTypes>
class TBaseRawMethodDelegateInstance<bConst, UserClass, RetValType(ParamTypes...)> : public IBaseDelegateInstance<RetValType(ParamTypes...)>
{
private:
    static_assert(!std::is_base_of_v<UObject, UserClass>, "UObject를 Raw 메소드 델리게이트와 사용할 수 없습니다.");
    
    using Super = IBaseDelegateInstance<RetValType(ParamTypes...)>;

public:
    using FMethodPtr = typename TMemFuncPtrType<bConst, UserClass, RetValType(ParamTypes...)>::Type;

    /**
     * @brief 새로운 인스턴스를 생성하고 초기화한다.
     *
     * @param InUserObject 멤버함수를 소유하고 있는 임의의 오브젝트 
     * @param InMethodFuncPtr 바인드할 C++ 멤버함수 포인터
     */
    explicit TBaseRawMethodDelegateInstance(UserClass* InUserObject, FMethodPtr InMethodFuncPtr)
     : MethodPtr(InMethodFuncPtr)
     , UserObject(InUserObject)
     , Handle(FDelegateHandle::GenerateNewHandle)
    {
        assert(InUserObject != nullptr && InMethodFuncPtr != nullptr);
    }
    
    /*-----------------------------------------------------------------------------
        IDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    UObject* GetUObject() const final
    {
        return nullptr;
    }

    bool IsSafeToExecute() const final
    {
        /**
         * C++ 포인터를 안전하게 역참조 가능한지는 절대 알 수 없다, 따라서 이 경우는 사용자를 신뢰할 수 밖에 없다.
         * 부득이한 경우가 아니라면 shared-pointer 기반 델리게이트를 사용하는 것을 추천한다!
         */
        return true;
    }

    FDelegateHandle GetHandle() const final
    {
        return Handle;
    }

    /*-----------------------------------------------------------------------------
        IBaseDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    RetValType Execute(ParamTypes... Params) const final
    {
        assert(UserObject != nullptr && MethodPtr != nullptr);

        return (UserObject->*MethodPtr)(std::forward<ParamTypes>(Params)...);
    }

    bool ExecuteIfSafe(ParamTypes... Params) const final
    {
        assert(UserObject != nullptr && MethodPtr != nullptr);

        (void)(UserObject->*MethodPtr)(std::forward<ParamTypes>(Params)...);

        return true;
    }

private:
    // 호출할 메소드를 보유하고있는 클래스에 대한 포인터
    UserClass* UserObject;

    // C++ 멤버함수 포인터
    FMethodPtr MethodPtr;
    
    FDelegateHandle Handle; 
};

/*-----------------------------------------------------------------------------
    TBaseSPMethodDelegateInstance
 -----------------------------------------------------------------------------*/

template <bool bConst, class UserClass, typename FuncType>
class TBaseSPMethodDelegateInstance;

/**
 * @brief std::shared_ptr로 관리되는 C++ 객체의 멤버 함수에 대한 바인딩이다.
 */
template <bool bConst, class UserClass, typename RetValType, typename... ParamTypes>
class TBaseSPMethodDelegateInstance<bConst, UserClass, RetValType(ParamTypes...)> : public IBaseDelegateInstance<RetValType(ParamTypes...)>
{
private:
    using Super = IBaseDelegateInstance<RetValType(ParamTypes...)>;

public:
    using FMethodPtr = typename TMemFuncPtrType<bConst, UserClass, RetValType(ParamTypes...)>::Type;

    /**
     * @note Shared pointer 델리게이트는 nullptr을 허용한다. Weak pointer가 만료될 경우,
     *       델리게이트 인스턴스의 복제본은 null pointer를 가질 수 있기 때문이다(이는 언리얼 엔진의 설명으로, 가급적이면 언리얼 엔진의 표준을 따른다).
     */
    explicit TBaseSPMethodDelegateInstance(std::shared_ptr<UserClass> InUserObject, FMethodPtr InMethodFuncPtr)
     : MethodPtr(InMethodFuncPtr)
     , UserObject(InUserObject)
     , Handle(FDelegateHandle::GenerateNewHandle)
    {
        assert(InMethodFuncPtr != nullptr);
    }
    
    /*-----------------------------------------------------------------------------
        IDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    UObject* GetUObject() const final
    {
        return nullptr;
    }

    bool IsSafeToExecute() const final
    {
        return !UserObject.expired();
    }

    FDelegateHandle GetHandle() const final
    {
        return Handle;
    }

    /*-----------------------------------------------------------------------------
        IBaseDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    RetValType Execute(ParamTypes... Params) const final
    {
        std::shared_ptr<UserClass> SharedUserObject = UserObject.lock();
        assert(SharedUserObject != nullptr);
        
        assert(MethodPtr != nullptr);

        return (SharedUserObject->*MethodPtr)(std::forward<ParamTypes>(Params)...);
    }

    bool ExecuteIfSafe(ParamTypes... Params) const final
    {
        if (auto ActualUserObject = UserObject.lock())
        {
            assert(MethodPtr != nullptr);

            (void)(ActualUserObject->*MethodPtr)(std::forward<ParamTypes>(Params)...);

            return true;
        }
        return false;
    }

private:
    // 호출할 메소드를 보유하고있는 클래스에 대한 약한 참조
    std::weak_ptr<UserClass> UserObject;

    // C++ 멤버함수 포인터
    FMethodPtr MethodPtr;
    
    FDelegateHandle Handle;  
};

/*-----------------------------------------------------------------------------
    TBaseFunctorDelegateInstance
 -----------------------------------------------------------------------------*/

template <typename FuncType, typename FunctorType>
class TBaseFunctorDelegateInstance;

template <typename RetValType, typename... ParamTypes, typename FunctorType>
class TBaseFunctorDelegateInstance<RetValType(ParamTypes...), FunctorType> : public IBaseDelegateInstance<RetValType(ParamTypes...)>
{
private:
    static_assert(!std::is_reference_v<FunctorType>, "FunctorType은 레퍼런스가 될 수 없습니다.");
    
    using Super = IBaseDelegateInstance<RetValType(ParamTypes...)>;
    
public:
    template <typename InFunctorType>
    TBaseFunctorDelegateInstance(InFunctorType&& InFunctor)
        : Functor(std::forward<InFunctorType>(InFunctor))
        , Handle(FDelegateHandle::GenerateNewHandle)
    {
    }

    /*-----------------------------------------------------------------------------
        IDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    UObject* GetUObject() const final
    {
        return nullptr;
    }

    bool IsSafeToExecute() const final
    {
        /** Functor는 항상 안전하게 실행가능 */
        return true;
    }

    FDelegateHandle GetHandle() const final
    {
        return Handle;
    }
    
    /*-----------------------------------------------------------------------------
        IBaseDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    RetValType Execute(ParamTypes... Params) const final
    {
        return Functor(std::forward<ParamTypes>(Params)...);
    }

    bool ExecuteIfSafe(ParamTypes... Params) const final
    {
        (void)Functor(std::forward<ParamTypes>(Params)...);
        
        return true;
    }

private:
    // C++ functor
    // mutable 람다를 바운드하고 실행하기 위해 mutable로 선언한다. 만약 const 델리게이트 객체를 복사한다면,
    // const 속성이 전파되므로 이를 피하기 위해 const를 제거한다.
    mutable std::remove_const_t<FunctorType> Functor;
    
    FDelegateHandle Handle;  
};

/*-----------------------------------------------------------------------------
    TWeakBaseFunctorDelegateInstance
 -----------------------------------------------------------------------------*/

template <typename FuncType, typename FunctorType>
class TWeakBaseFunctorDelegateInstance;

template <typename RetValType, typename... ParamTypes, typename FunctorType>
class TWeakBaseFunctorDelegateInstance<RetValType(ParamTypes...), FunctorType> : public IBaseDelegateInstance<RetValType(ParamTypes...)>
{
private:
    static_assert(!std::is_reference_v<FunctorType>, "FunctorType은 레퍼런스가 될 수 없습니다.");
    
    using Super = IBaseDelegateInstance<RetValType(ParamTypes...)>;

    /*
     * 템플릿 파싱 단계(Phase 1)에서 UObject가 '불완전한 타입(incomplete type)'으로
     * 간주되어 발생하는 컴파일 오류를 회피하기 위한 목적이다.
     *
     * std::conditional_t를 사용함으로써, UserClass의 타입 정의를
     * 템플릿 파라미터 'ParamTypes'에 의존(dependent)하도록 만든다.
     *
     * 결과적으로 컴파일러는 UObject 타입의 유효성 검사를
     * 템플릿이 실제 코드로 생성되는 인스턴스화 단계(Phase 2)로 지연시키게 된다.
     *
     * (이 조건문은 항상 'const UObject'를 반환하지만,
     * 컴파일러의 타입 확인 시점을 늦추는 역할만 수행한다.)
     */
    using UserClass = std::conditional_t<sizeof...(ParamTypes) == 0, const UObject, const UObject>;
    
public:
    template <typename InFunctorType>
    TWeakBaseFunctorDelegateInstance(UserClass* InContextObject, InFunctorType&& InFunctor)
        : ContextObject(const_cast<UObject*>(InContextObject))
        , Functor(std::forward<InFunctorType>(InFunctor))
        , Handle(FDelegateHandle::GenerateNewHandle)
    {
    }

    /*-----------------------------------------------------------------------------
        IDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    UObject* GetUObject() const final
    {
        return ContextObject.Get();
    }

    bool IsSafeToExecute() const final
    {
        return ContextObject.IsValid();
    }

    FDelegateHandle GetHandle() const final
    {
        return Handle;
    }
    
    /*-----------------------------------------------------------------------------
        IBaseDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    RetValType Execute(ParamTypes... Params) const final
    {
        return Functor(std::forward<ParamTypes>(Params)...);
    }

    bool ExecuteIfSafe(ParamTypes... Params) const final
    {
        // WeakPtr 유효성 체크 - ContextObject가 파괴되었으면 실행하지 않음
        if (!IsSafeToExecute())
        {
            return false;
        }

        (void)Functor(std::forward<ParamTypes>(Params)...);

        return true;
    }

private:
    FWeakObjectPtr ContextObject;
    
    // C++ functor
    // mutable 람다를 바운드하고 실행하기 위해 mutable로 선언한다. 만약 const 델리게이트 객체를 복사한다면,
    // const 속성이 전파되므로 이를 피하기 위해 const를 제거한다.
    mutable std::remove_const_t<FunctorType> Functor;
    
    FDelegateHandle Handle;   
};

/*-----------------------------------------------------------------------------
    TBaseUObjectMethodDelegateInstance
 -----------------------------------------------------------------------------*/

template <bool bConst, class UserClass, typename FuncType>
class TBaseUObjectMethodDelegateInstance;

/**
 * @brief UObject 객체의 멤버 함수에 대한 바인딩이다. (FWeakObjectPtr로 안전성 확보)
 *
 * @todo TWeakObjectPtr을 사용하여 타입 안정성과 효율을 확보환다.
 */
template <bool bConst, class UserClass, typename RetValType, typename... ParamTypes>
class TBaseUObjectMethodDelegateInstance<bConst, UserClass, RetValType(ParamTypes...)> : public IBaseDelegateInstance<RetValType(ParamTypes...)>
{
private:
    using Super = IBaseDelegateInstance<RetValType(ParamTypes...)>;

    static_assert(std::is_base_of_v<UObject, UserClass>, "UObject 메소드 델리게이트를 Raw 포인터와 사용할 수 없습니다.");

public:
    using FMethodPtr = typename TMemFuncPtrType<bConst, UserClass, RetValType(ParamTypes...)>::Type;

    /**
     * @note UObject 델리게이트는 nullptr을 허용한다. UObject weak pointer가 만료될 경우,
     *       델리게이트 인스턴스의 복제본은 null pointer를 가질 수 있기 때문이다(이는 언리얼 엔진의 설명으로, 가급적이면 언리얼 엔진의 표준을 따른다).
     */
    explicit TBaseUObjectMethodDelegateInstance(UserClass* InUserObject, FMethodPtr InMethodPtr)
        : UserObject(InUserObject)
        , MethodPtr(InMethodPtr)
        , Handle(FDelegateHandle::GenerateNewHandle)
    {
        assert(MethodPtr != nullptr); 
    }
    
    /*-----------------------------------------------------------------------------
        IDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    UObject* GetUObject() const final
    {
        return UserObject.Get();
    }

    bool IsSafeToExecute() const final
    {
        return UserObject.IsValid();
    }

    FDelegateHandle GetHandle() const final
    {
        return Handle;
    }

    /*-----------------------------------------------------------------------------
        IBaseDelegateInstance 인터페이스
     -----------------------------------------------------------------------------*/

    RetValType Execute(ParamTypes... Params) const final
    {
        // UserObject에 대한 weak pointer만을 가지고 있기 때문에 유효한지 검증한다.
        assert(UserObject.IsValid());

        assert(MethodPtr != nullptr);

        return (UserObject->*MethodPtr)(std::forward<ParamTypes>(Params)...);
    }

    bool ExecuteIfSafe(ParamTypes... Params) const final
    {
        if (UserClass* ActualUserObject = static_cast<UserClass*>(UserObject.Get()))
        {
            assert(MethodPtr != nullptr);

            (void)(ActualUserObject->*MethodPtr)(std::forward<ParamTypes>(Params)...);

            return true;
        }
        return false;
    }

private:
    // 호출할 메소드를 보유하고있는 클래스에 대한 포인터
    FWeakObjectPtr UserObject;

    // C++ 멤버함수 포인터
    FMethodPtr MethodPtr;
    
    FDelegateHandle Handle; 
};