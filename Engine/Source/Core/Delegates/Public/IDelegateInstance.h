#pragma once

/*-----------------------------------------------------------------------------
    FDelegateHandle
 -----------------------------------------------------------------------------*/

/**
 * @brief 델리게이트에 바인딩된 특정 객체/함수 쌍을 나타내는 핸들 클래스이다.
 */
class FDelegateHandle
{
public:
    enum EGenerateNewHandleType
    {
        GenerateNewHandle
    };

    /** 초기에 설정되지 않은 핸들(ID=0)을 생성한다. */
    FDelegateHandle()
        : ID(0)
    {
    }

    /** 새 인스턴스를 가리키는 핸들을 생성한다. (고유 ID 생성) */
    explicit FDelegateHandle(EGenerateNewHandleType)
        : ID(GenerateNewID())
    {
    }

    /**
     * 이 핸들이 델리게이트에 한 번이라도 바인딩되었다면 true를 반환한다.
     * (단, 현재도 유효한지 확인하려면 반드시 소유 델리게이트 측에 확인해야 한다.)
     */
    bool IsValid() const
    {
        return ID != 0;
    }

    /** 핸들을 초기화하여 더 이상 바인딩되지 않았음을 나타낸다. */
    void Reset()
    {
        ID = 0;
    }

    bool operator==(const FDelegateHandle& Rhs) const
    {
        return ID == Rhs.ID;
    }

    bool operator!=(const FDelegateHandle& Rhs) const
    {
        return ID != Rhs.ID;
    }

private:
    /**
     * 다음 델리게이트 ID 생성을 위한 전역 카운터이다.
     * (UE 원본에서는 .cpp 파일 내부의 UE::Delegates::Private 네임스페이스에 정의되어 
     * internal linkage로 관리되는 변수에 해당한다.)
     */
    static uint64 GNextID;

    /**
     * 델리게이트 핸들에 사용할 새 ID를 생성한다.
     *
     * @return 델리게이트를 위한 고유 ID이다.
     */
    static uint64 GenerateNewID()
    {
        return ++GNextID;
    }

    uint64 ID;
};

/*-----------------------------------------------------------------------------
    IDelegateInstance
 -----------------------------------------------------------------------------*/

using FDelegateUserObject = void*;
using FDelegateUserObjectConst = const void*;

class IDelegateInstance
{
public:
    virtual ~IDelegateInstance() = default;

    /**
     * @brief 이 델리게이트 인스턴스가 바인딩된 UObject를 반환한다.
     *
     * @return UObject에 대한 포인터이다. UObject에 바인딩되지 않은 경우 nullptr이다.
     */
    virtual UObject* GetUObject() const = 0;

    /**
     * @brief 이 델리게이트에 바인딩된 사용자 객체(User Object)가 여전히 유효한지 확인한다.
     *
     * @return 사용자 객체가 여전히 유효하여 함수 호출을 실행해도 안전한 경우 true이다.
     */
    virtual bool IsSafeToExecute() const = 0;

    /**
     * @brief 이 델리게이트의 핸들을 반환한다.
     */
    virtual FDelegateHandle GetHandle() const = 0;
};

/*-----------------------------------------------------------------------------
    IBaseDelegateInstance (UE: DelegateInstanceInterface.h)
 -----------------------------------------------------------------------------*/

template <typename FuncType>
class IBaseDelegateInstance;

/**
 * @brief 델리게이트 인스턴스의 추상 베이스 클래스 (인터페이스).
 *
 * 델리게이트 시스템의 Type Erasure(타입 소거)를 위한 핵심 인터페이스이다.
 * 람다, 멤버 함수 포인터, 전역 함수 등 다양한 호출 가능 객체(Callable)를
 * 동일한 시그니처(RetType(ArgTypes...)) 하에 추상화하여 저장하고 실행할 수 있도록 한다.
 *
 * TDelegate 또는 TMulticastDelegate는 이 인터페이스에 대한 포인터를 유지하여
 * 실제 바인딩된 구현을 호출한다.
 */
template <typename RetType, typename... ArgTypes>
class IBaseDelegateInstance<RetType(ArgTypes...)> : public IDelegateInstance
{
public:
    /**
     * @brief 델리게이트를 실행한다. 함수 포인터가 유효하지 않을 경우, 오류가 발생할 수 있다.
     */
    virtual RetType Execute(ArgTypes...) const = 0;

    /**
     * @brief 오직 함수 포인터가 유효할 경우에만 델리게이트를 실행한다.
     * 
     * @return 함수가 실행되었을 경우에만 true를 반환한다.
     * 
     * @note 현재 반환값이 없는 델리게이트만 ExecuteIfSafe를 지원한다.
     */ 
    virtual bool ExecuteIfSafe(ArgTypes...) const = 0;
};

template <bool Const, typename Class, typename FuncType>
struct TMemFuncPtrType;

template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFuncPtrType<false, Class, RetType(ArgTypes...)>
{
    typedef RetType (Class::* Type)(ArgTypes...);
};

template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFuncPtrType<true, Class, RetType(ArgTypes...)>
{
    typedef RetType (Class::* Type)(ArgTypes...) const;
};