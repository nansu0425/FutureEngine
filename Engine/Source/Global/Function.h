#pragma once

using std::string;
using std::wstring;

// ============================================================================
// Math Utility Functions
// ============================================================================

/**
 * @brief 선형 보간 (Linear Interpolation)
 * @tparam T 보간할 타입 (float, FVector, FQuaternion 등)
 * @param A 시작 값
 * @param B 끝 값
 * @param Alpha 보간 비율 [0, 1]
 * @return 보간된 값
 */
template <typename T>
T Lerp(const T& A, const T& B, float Alpha)
{
	return A * (1.0f - Alpha) + B * Alpha;
}

/**
 * @brief 값을 min과 max 사이로 제한
 * @tparam T 제한할 타입
 * @param Value 제한할 값
 * @param Min 최소값
 * @param Max 최대값
 * @return 제한된 값
 */
template <typename T>
T Clamp(const T& Value, const T& Min, const T& Max)
{
	return (Value < Min) ? Min : (Value > Max) ? Max : Value;
}

// ============================================================================
// Memory & COM Utilities
// ============================================================================

template <typename T>
static void SafeDelete(T& InDynamicObject)
{
	delete InDynamicObject;
	InDynamicObject = nullptr;
}

template <typename T>
void SafeRelease(T*& ComObj)
{
	if (ComObj != nullptr)
	{
		ComObj->Release();
		ComObj = nullptr;
	}
}

/**
 * @brief wstring을 멀티바이트 FString으로 변환합니다.
 * @param InString 변환할 FString
 * @return 변환된 wstring
 */
static FString WideStringToString(const wstring& InString)
{
	int32 ByteNumber = WideCharToMultiByte(CP_UTF8, 0,
	                                     InString.c_str(), -1, nullptr, 0, nullptr, nullptr);

	FString OutString(ByteNumber, 0);

	WideCharToMultiByte(CP_UTF8, 0,
	                    InString.c_str(), -1, OutString.data(), ByteNumber, nullptr, nullptr);

	return OutString;
}

/**
 * @brief 멀티바이트 UTF-8 FString을 와이드 스트링(wstring)으로 변환합니다.
 * @param InString 변환할 FString (UTF-8)
 * @return 변환된 wstring
 */
static wstring StringToWideString(const FString& InString)
{
	// 필요한 와이드 문자의 개수를 계산
	int32 WideCharNumber = MultiByteToWideChar(CP_UTF8, 0,
										  InString.c_str(), -1, nullptr, 0);

	// 계산된 크기만큼 wstring 버퍼를 할당
	wstring OutString(WideCharNumber, 0);

	// 실제 변환을 수행
	MultiByteToWideChar(CP_UTF8, 0,
						InString.c_str(), -1, OutString.data(), WideCharNumber);

	// null 문자 제거
	OutString.resize(WideCharNumber - 1);

	return OutString;
}

/**
 * @brief CP949 (ANSI) 타입의 문자열을 UTF-8 타입으로 변환하는 함수
 * CP949 To UTF-8 직변환 함수가 제공되지 않아 UTF-16을 브릿지로 사용한다
 * @param InANSIString ANSI 문자열
 * @return UTF-8 문자열
 */
static string ConvertCP949ToUTF8(const char* InANSIString)
{
	if (InANSIString == nullptr || InANSIString[0] == '\0')
	{
		return "";
	}

	// CP949 -> UTF-8 변환
	int WideCharacterSize = MultiByteToWideChar(CP_ACP, 0, InANSIString, -1, nullptr, 0);
	if (WideCharacterSize == 0)
	{
		return "";
	}

	wstring WideString(WideCharacterSize, 0);
	MultiByteToWideChar(CP_ACP, 0, InANSIString, -1, WideString.data(), WideCharacterSize);

	// UTF-16 -> UTF-8 변환
	int UTF8Size = WideCharToMultiByte(CP_UTF8, 0, WideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (UTF8Size == 0)
	{
		return "";
	}

	string UTF8String(UTF8Size, 0);
	WideCharToMultiByte(CP_UTF8, 0, WideString.c_str(), -1, UTF8String.data(), UTF8Size, nullptr, nullptr);

	if (!UTF8String.empty() && UTF8String.back() == '\0')
	{
		UTF8String.pop_back();
	}

	return UTF8String;
}
