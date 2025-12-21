#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/CoreLib.h"
#include "rkit/Core/Platform.h"

#if RKIT_PLATFORM_ARCH_HAVE_SSE
#include <immintrin.h>
#endif

namespace rkit { namespace math {
	float RKIT_CORELIB_API SoftwareSqrtf(float f);
	double RKIT_CORELIB_API SoftwareSqrt(double f);
} }

namespace rkit { namespace math {
	float Sqrtf(float f);
	double Sqrt(double f);

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

#if RKIT_PLATFORM_ARCH_HAVE_SSE2
#include <emmintrin.h>
#endif

#if RKIT_PLATFORM_ARCH_FAMILY == RKIT_PLATFORM_ARCH_FAMILY_X86
#include <intrin.h>
#endif

namespace rkit { namespace math {
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

#if RKIT_PLATFORM_ARCH_HAVE_SSE
	inline float Sqrtf(float f)
	{
		return _mm_cvtss_f32(_mm_sqrt_ss(_mm_load_ss(&f)));
	}

	inline float FastRsqrtNonDeterministic(float value)
	{
		return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_load_ss(&value)));
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
} }
