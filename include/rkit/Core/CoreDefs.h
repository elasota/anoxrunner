#pragma once

#include <cstddef>
#include <cstdint>

#if __cplusplus >= 201703L
#define RKIT_NODISCARD [[nodiscard]]
#else
#define RKIT_NODISCARD
#endif

#define RKIT_PP_CONCAT2(expr1, expr2) expr1 ## expr2
#define RKIT_PP_CONCAT(expr1, expr2) RKIT_PP_CONCAT2(expr1, expr2)

#if defined(_MSC_VER) || defined(__clang__) || defined(__GNUC__)
#define RKIT_RESTRICT __restrict
#else
#define RKIT_RESTRICT
#endif

#define RKIT_STATIC_ASSERT(expr) static_assert((expr), #expr)


#ifdef _MSC_VER
#pragma warning(error:4715)	// Not all control paths return a value
#pragma warning(error:4834)	// [[nodiscard]] discarded
#endif


#define RKIT_PLATFORM_WIN32	1

#if RKIT_IS_DEBUG
	#define RKIT_USE_CLASS_RESULT 1
#else
	#define RKIT_USE_CLASS_RESULT 0
#endif

#if RKIT_USE_CLASS_RESULT

namespace rkit
{
	struct Result;
}

#else

namespace rkit
{
	enum class Result : uint64_t;
}

#endif
