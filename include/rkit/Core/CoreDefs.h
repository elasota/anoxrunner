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
