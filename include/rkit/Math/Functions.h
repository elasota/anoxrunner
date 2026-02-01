#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/CoreLib.h"
#include "rkit/Core/Platform.h"

#if RKIT_PLATFORM_ARCH_HAVE_SSE != 0
#include <immintrin.h>
#endif

namespace rkit { namespace math { namespace priv {
	template<class T>
	struct SamePrecisionMathHelper
	{
	};

	template<>
	struct SamePrecisionMathHelper<float>
	{
		static float Sqrt(float f);
		static float Abs(float f);
		static float Floor(float f);
	};

	template<>
	struct SamePrecisionMathHelper<double>
	{
		static double Sqrt(double f);
		static double Abs(double f);
		static double Floor(double f);
	};
} } }

namespace rkit { namespace math {
	float Sqrtf(float f);
	double Sqrt(double f);

	float Absf(float f);
	double Abs(double f);

	float Floorf(float f);
	double Floor(double f);

	template<class T>
	T SamePrecisionSqrt(const T &value);

	template<class T>
	T SamePrecisionAbs(const T &value);

	template<class T>
	T SamePrecisionFloor(const T &value);

	float FastRsqrtDeterministic(float value);
	float FastRsqrtNonDeterministic(float value);

	int64_t SignedMultiplyInt64High(int64_t a, int64_t b);
	void SignedMultiplyInt64(int64_t a, int64_t b, int64_t &outLow, int64_t &outHigh);

	uint64_t UnsignedMultiplyInt64High(uint64_t a, uint64_t b);
	void UnsignedMultiplyInt64(uint64_t a, uint64_t b, uint64_t &outLow, uint64_t &outHigh);

	int32_t SignedMultiplyInt32High(int32_t a, int32_t b);
	uint32_t UnsignedMultiplyInt32High(uint32_t a, uint32_t b);
} }

#include <string.h>
#include <math.h>

#if RKIT_PLATFORM_ARCH_HAVE_SSE != 0
#include <xmmintrin.h>
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE2 != 0
#include <emmintrin.h>
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
#include <smmintrin.h>
#endif

#if RKIT_PLATFORM_ARCH_FAMILY == RKIT_PLATFORM_ARCH_FAMILY_X86
#include <intrin.h>
#endif

namespace rkit { namespace math { namespace priv {
	inline float SamePrecisionMathHelper<float>::Sqrt(float f)
	{
		return math::Sqrtf(f);
	}

	inline float SamePrecisionMathHelper<float>::Abs(float f)
	{
		return math::Absf(f);
	}

	inline float SamePrecisionMathHelper<float>::Floor(float f)
	{
		return math::Floorf(f);
	}

	inline double SamePrecisionMathHelper<double>::Sqrt(double f)
	{
		return math::Sqrt(f);
	}

	inline double SamePrecisionMathHelper<double>::Abs(double f)
	{
		return math::Abs(f);
	}

	inline double SamePrecisionMathHelper<double>::Floor(double f)
	{
		return math::Floor(f);
	}
} } }

namespace rkit { namespace math {

	template<class T>
	T SamePrecisionSqrt(const T &value)
	{
		return priv::SamePrecisionMathHelper<T>::Sqrt(value);
	}

	template<class T>
	T SamePrecisionAbs(const T &value)
	{
		return priv::SamePrecisionMathHelper<T>::Abs(value);
	}

	template<class T>
	T SamePrecisionFloor(const T &value)
	{
		return priv::SamePrecisionMathHelper<T>::Floor(value);
	}

	// Jan Kadlec - Improving the fast inverse square root
	inline float FastRsqrtDeterministic(float value)
	{
		uint32_t u = 0;
		memcpy(&u, &value, 4);
		u = 0x5F1FFFF9u - (u >> 1);

		float f = 0.f;
		memcpy(&f, &u, 4);
		return (f * 0.703952253f) * (2.38924456f - value * f * f);
	}

	inline int32_t SignedMultiplyInt32High(int32_t a, int32_t b)
	{
		return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 32);
	}

	inline uint32_t UnsignedMultiplyInt32High(uint32_t a, uint32_t b)
	{
		return static_cast<uint32_t>((static_cast<int64_t>(a) * b) >> 32);
	}

#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
	inline double Floor(double f)
	{
		return _mm_cvtsd_f64(_mm_floor_sd(_mm_setzero_pd(), _mm_load_sd(&f)));
	}
#else
	inline double Floor(double f)
	{
		return ::floor(f);
	}
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
	inline float Floorf(float f)
	{
		return _mm_cvtss_f32(_mm_floor_ss(_mm_setzero_ps(), _mm_load_ss(&f)));
	}
#else
	inline float Floorf(float f)
	{
		return ::floorf(f);
	}
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE2 != 0
	inline double Sqrt(double f)
	{
		return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_load_sd(&f)));
	}
#else
	inline double Sqrt(double f)
	{
		return ::sqrt(f);
	}
#endif

#if RKIT_PLATFORM_ARCH_HAVE_SSE != 0
	inline float Sqrtf(float f)
	{
		return _mm_cvtss_f32(_mm_sqrt_ss(_mm_load_ss(&f)));
	}

	inline float FastRsqrtNonDeterministic(float value)
	{
		return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_load_ss(&value)));
	}
#else
	inline float Sqrtf(float f)
	{
		return ::sqrtf(f);
	}

	inline float FastRsqrtNonDeterministic(float value)
	{
		return FastRsqrtDeterministic(value);
	}
#endif

#if RKIT_PLATFORM_ARCH == RKIT_PLATFORM_ARCH_X64
	inline int64_t SignedMultiplyInt64High(int64_t a, int64_t b)
	{
		return __mulh(a, b);
	}

	inline void SignedMultiplyInt64(int64_t a, int64_t b, int64_t &outLow, int64_t &outHigh)
	{
		outLow = _mul128(a, b, &outHigh);
	}

	inline uint64_t UnsignedMultiplyInt64High(uint64_t a, uint64_t b)
	{
		return __umulh(a, b);
	}

	inline void UnsignedMultiplyInt64(uint64_t a, uint64_t b, uint64_t &outLow, uint64_t &outHigh)
	{
		outLow = _umul128(a, b, &outHigh);
	}
#else
	inline int64_t SignedMultiplyInt64High(int64_t a, int64_t b)
	{
		int64_t discard = 0;
		int64_t high = 0;
		SignedMultiplyInt64(a, b, discard, high);
		return high;
	}

	inline void SignedMultiplyInt64(int64_t a, int64_t b, int64_t &outLow, int64_t &outHigh)
	{
		//   asx asx ahi alo
		// * bsx bsx bhi blo
		//--------------------------
		// (asx asx ahi alo) * blo
		// (asx ahi alo)     * bhi
		// (ahi alo)         * bsx
		// (alo)             * bsx
		const uint32_t alo = static_cast<uint64_t>(a) & 0xffffffffu;
		const uint32_t ahi = (static_cast<uint64_t>(a) >> 32) & 0xffffffffu;
		const uint32_t blo = static_cast<uint64_t>(b) & 0xffffffffu;
		const uint32_t bhi = (static_cast<uint64_t>(b) >> 32) & 0xffffffffu;
		const uint32_t asx = static_cast<uint64_t>(a >> 63) & 0xffffffffu;
		const uint32_t bsx = static_cast<uint64_t>(a >> 63);

		const uint64_t accum0 = static_cast<uint64_t>(alo) * blo;
		const uint64_t accum1 = static_cast<uint64_t>(accum0 >> 32)
			+ static_cast<uint64_t>(ahi) * blo
			+ static_cast<uint64_t>(alo) * bhi;
		const uint64_t accum2 = static_cast<uint64_t>(accum1 >> 32)
			+ static_cast<uint64_t>(asx) * blo
			+ static_cast<uint64_t>(ahi) * bhi
			+ static_cast<uint64_t>(alo) * bsx;
		const uint64_t accum3 = static_cast<uint64_t>(accum2 >> 32)
			+ static_cast<uint64_t>(asx) * blo
			+ static_cast<uint64_t>(asx) * bhi
			+ static_cast<uint64_t>(ahi) * bsx
			+ static_cast<uint64_t>(alo) * bsx;

		const uint64_t resultLo = (accum0 & 0xffffffffu) | ((accum1 & 0xffffffffu) << 32);
		const uint64_t resultHi = (accum2 & 0xffffffffu) | ((accum3 & 0xffffffffu) << 32);

		outLow = static_cast<int64_t>(resultLo);
		outHigh = static_cast<int64_t>(resultHi);
	}

	inline uint64_t UnsignedMultiplyInt64High(uint64_t a, uint64_t b)
	{
		uint64_t discard = 0;
		uint64_t high = 0;
		UnsignedMultiplyInt64(a, b, discard, high);
		return high;
	}

	inline void UnsignedMultiplyInt64(uint64_t a, uint64_t b, uint64_t &outLow, uint64_t &outHigh)
	{
		//   asx asx ahi alo
		// * bsx bsx bhi blo
		//--------------------------
		//         (ahi alo) * blo
		//     (ahi alo)     * bhi
		const uint32_t alo = static_cast<uint64_t>(a) & 0xffffffffu;
		const uint32_t ahi = (static_cast<uint64_t>(a) >> 32) & 0xffffffffu;
		const uint32_t blo = static_cast<uint64_t>(b) & 0xffffffffu;
		const uint32_t bhi = (static_cast<uint64_t>(b) >> 32) & 0xffffffffu;

		const uint64_t accum0 = static_cast<uint64_t>(alo) * blo;
		const uint64_t accum1 = static_cast<uint64_t>(accum0 >> 32)
			+ static_cast<uint64_t>(ahi) * blo
			+ static_cast<uint64_t>(alo) * bhi;
		const uint64_t accum2 = static_cast<uint64_t>(accum1 >> 32)
			+ static_cast<uint64_t>(ahi) * bhi;

		const uint64_t resultLo = (accum0 & 0xffffffffu) | ((accum1 & 0xffffffffu) << 32);

		outLow = static_cast<int64_t>(resultLo);
		outHigh = static_cast<int64_t>(accum2);
	}
#endif

	inline float Absf(float f)
	{
		return ::fabsf(f);
	}

	inline double Abs(double f)
	{
		return ::fabs(f);
	}
} }
