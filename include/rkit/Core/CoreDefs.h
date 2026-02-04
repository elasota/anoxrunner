#pragma once

#include <cstddef>
#include <cstdint>

#if __cplusplus >= 201703L
#define RKIT_NODISCARD [[nodiscard]]
#else
#define RKIT_NODISCARD
#endif

#if __cplusplus >= 202002L
#define RKIT_CPP20 1
#else
#define RKIT_CPP20 0
#endif

#define RKIT_PP_CONCAT2(expr1, expr2) expr1 ## expr2
#define RKIT_PP_CONCAT(expr1, expr2) RKIT_PP_CONCAT2(expr1, expr2)

#if defined(_MSC_VER) || defined(__clang__) || defined(__GNUC__)
#define RKIT_RESTRICT __restrict
#else
#define RKIT_RESTRICT
#endif

#if defined(__clang__) || defined(__GNUC__)
#define RKIT_MAY_ALIAS __attribute__((__may_alias__))
#else
#define RKIT_MAY_ALIAS
#endif


#define RKIT_STATIC_ASSERT(expr) static_assert((expr), #expr)

#if !!RKIT_IS_DEBUG
#	define RKIT_DEBUG_ONLY(...) __VA_ARGS__
#else
#	define RKIT_DEBUG_ONLY(...)
#endif

#ifdef _MSC_VER
#pragma warning(error:4715)	// Not all control paths return a value
#pragma warning(error:4834)	// [[nodiscard]] discarded
#endif

// Platforms
#define RKIT_PLATFORM_WIN32	1


#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32
#define RKIT_DLLEXPORT_API __declspec(dllexport)
#define RKIT_DLLIMPORT_API __declspec(dllimport)
#else
#define RKIT_DLLEXPORT_API
#define RKIT_DLLIMPORT_API
#endif

#define RKIT_RESULT_BEHAVIOR_ENUM 1
#define RKIT_RESULT_BEHAVIOR_CLASS 2
#define RKIT_RESULT_BEHAVIOR_EXCEPTION 3

#if RKIT_IS_DEBUG != 0
	#define RKIT_RESULT_BEHAVIOR RKIT_RESULT_BEHAVIOR_EXCEPTION
#else
	#define RKIT_RESULT_BEHAVIOR RKIT_RESULT_BEHAVIOR_ENUM
#endif

#define RKIT_SIMD_ALIGNMENT	16

#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
namespace rkit { namespace mem {
	constexpr size_t kDefaultAllocAlignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
} }
#else
namespace rkit { namespace mem {
	constexpr size_t kDefaultAllocAlignment = RKIT_SIMD_ALIGNMENT;
} }
#endif


#if RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_CLASS

namespace rkit
{
	struct Result;
}

#elif RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_ENUM

namespace rkit
{
	enum class Result : uint64_t;
}

#elif RKIT_RESULT_BEHAVIOR == RKIT_RESULT_BEHAVIOR_EXCEPTION

namespace rkit
{
	typedef void Result;
}

#endif

namespace rkit
{
	typedef char16_t Utf16Char_t;
	typedef char32_t Utf32Char_t;
}

#if !!RKIT_CPP20
namespace rkit
{
	typedef char8_t Utf8Char_t;
}
#else
namespace rkit
{
	typedef char Utf8Char_t;
}
#endif


namespace rkit
{
#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32
	typedef char16_t OSPathChar_t;
#else
	typedef char OSPathChar_t;
#endif
}

namespace rkit
{
	inline char *ReinterpretUtf8CharToAnsiChar(Utf8Char_t *chars)
	{
		return reinterpret_cast<char *>(chars);
	}
	inline const char *ReinterpretUtf8CharToAnsiChar(const Utf8Char_t *chars)
	{
		return reinterpret_cast<const char *>(chars);
	}
	inline volatile char *ReinterpretUtf8CharToAnsiChar(volatile Utf8Char_t *chars)
	{
		return reinterpret_cast<volatile char *>(chars);
	}
	inline const volatile char *ReinterpretUtf8CharToAnsiChar(const volatile Utf8Char_t *chars)
	{
		return reinterpret_cast<const volatile char *>(chars);
	}

	inline Utf8Char_t *ReinterpretAnsiCharToUtf8Char(Utf8Char_t *chars)
	{
		return reinterpret_cast<Utf8Char_t *>(chars);
	}
	inline const Utf8Char_t *ReinterpretAnsiCharToUtf8Char(const char *chars)
	{
		return reinterpret_cast<const Utf8Char_t *>(chars);
	}
	inline volatile Utf8Char_t *ReinterpretAnsiCharToUtf8Char(volatile char *chars)
	{
		return reinterpret_cast<volatile Utf8Char_t *>(chars);
	}
	inline const volatile Utf8Char_t *ReinterpretAnsiCharToUtf8Char(const volatile char *chars)
	{
		return reinterpret_cast<const volatile Utf8Char_t *>(chars);
	}
}
