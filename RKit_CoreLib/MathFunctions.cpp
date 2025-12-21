#include "rkit/Math/Functions.h"

#include <stdint.h>
#include <string.h>
#include <math.h>

/*
Based on musl

Copyright © 2005-2014 Rich Felker, et al.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

namespace rkit { namespace math { namespace priv {
	static const uint16_t kRsqrtTab[128] =
	{
		0xb451,0xb2f0,0xb196,0xb044,0xaef9,0xadb6,0xac79,0xab43,
		0xaa14,0xa8eb,0xa7c8,0xa6aa,0xa592,0xa480,0xa373,0xa26b,
		0xa168,0xa06a,0x9f70,0x9e7b,0x9d8a,0x9c9d,0x9bb5,0x9ad1,
		0x99f0,0x9913,0x983a,0x9765,0x9693,0x95c4,0x94f8,0x9430,
		0x936b,0x92a9,0x91ea,0x912e,0x9075,0x8fbe,0x8f0a,0x8e59,
		0x8daa,0x8cfe,0x8c54,0x8bac,0x8b07,0x8a64,0x89c4,0x8925,
		0x8889,0x87ee,0x8756,0x86c0,0x862b,0x8599,0x8508,0x8479,
		0x83ec,0x8361,0x82d8,0x8250,0x81c9,0x8145,0x80c2,0x8040,
		0xff02,0xfd0e,0xfb25,0xf947,0xf773,0xf5aa,0xf3ea,0xf234,
		0xf087,0xeee3,0xed47,0xebb3,0xea27,0xe8a3,0xe727,0xe5b2,
		0xe443,0xe2dc,0xe17a,0xe020,0xdecb,0xdd7d,0xdc34,0xdaf1,
		0xd9b3,0xd87b,0xd748,0xd61a,0xd4f1,0xd3cd,0xd2ad,0xd192,
		0xd07b,0xcf69,0xce5b,0xcd51,0xcc4a,0xcb48,0xca4a,0xc94f,
		0xc858,0xc764,0xc674,0xc587,0xc49d,0xc3b7,0xc2d4,0xc1f4,
		0xc116,0xc03c,0xbf65,0xbe90,0xbdbe,0xbcef,0xbc23,0xbb59,
		0xba91,0xb9cc,0xb90a,0xb84a,0xb78c,0xb6d0,0xb617,0xb560,
	};

	static uint64_t BitCastDoubleToU64(double d)
	{
		static_assert(sizeof(double) == 8, "double is the wrong size");

		uint64_t result = 0;
		memcpy(&result, &d, 8);
		return result;
	}

	static uint32_t BitCastFloatToU32(float f)
	{
		static_assert(sizeof(float) == 4, "float is the wrong size");

		uint32_t result = 0;
		memcpy(&result, &f, 4);
		return result;
	}

	static double BitCastU64ToDouble(uint64_t u)
	{
		static_assert(sizeof(double) == 8, "double is the wrong size");

		double result = 0;
		memcpy(&result, &u, 8);
		return result;
	}

	static float BitCastU32toFloat(uint32_t u)
	{
		static_assert(sizeof(float) == 4, "float is the wrong size");

		float result = 0;
		memcpy(&result, &u, 4);
		return result;
	}
} } }

namespace rkit { namespace math {
	float RKIT_CORELIB_API SoftwareSqrtf(float x)
	{
		uint32_t ix = priv::BitCastFloatToU32(x);
		if (ix - 0x00800000 >= 0x7f800000 - 0x00800000)
		{
			/* x < 0x1p-126 or inf or nan.  */
			if (ix * 2 == 0)
				return x;
			if (ix == 0x7f800000)
				return x;
			if (ix > 0x7f800000)
				return (x - x) / (x - x);
			/* x is subnormal, normalize it.  */
			ix = priv::BitCastFloatToU32(x * 0x1p23f);
			ix -= 23 << 23;
		}

		/* x = 4^e m; with int e and m in [1, 4).  */
		const uint32_t even = ix & 0x00800000;
		const uint32_t m1 = (ix << 8) | 0x80000000;
		const uint32_t m0 = (ix << 7) & 0x7fffffff;
		const uint32_t m = even ? m0 : m1;

		/* 2^e is the exponent part of the return value.  */
		uint32_t ey = ix >> 1;
		ey += 0x3f800000 >> 1;
		ey &= 0x7f800000;

		/* compute r ~ 1/sqrt(m), s ~ sqrt(m) with 2 goldschmidt iterations.  */
		static const uint32_t three = 0xc0000000;
		uint32_t r, s, d, u, i;
		i = (ix >> 17) % 128;
		r = static_cast<uint32_t>(priv::kRsqrtTab[i]) << 16;
		/* |r*sqrt(m) - 1| < 0x1p-8 */
		s = UnsignedMultiplyInt32High(m, r);
		/* |s/sqrt(m) - 1| < 0x1p-8 */
		d = UnsignedMultiplyInt32High(s, r);
		u = three - d;
		r = UnsignedMultiplyInt32High(r, u) << 1;
		/* |r*sqrt(m) - 1| < 0x1.7bp-16 */
		s = UnsignedMultiplyInt32High(s, u) << 1;
		/* |s/sqrt(m) - 1| < 0x1.7bp-16 */
		d = UnsignedMultiplyInt32High(s, r);
		u = three - d;
		s = UnsignedMultiplyInt32High(s, u);
		/* -0x1.03p-28 < s/sqrt(m) - 1 < 0x1.fp-31 */
		s = (s - 1) >> 6;
		/* s < sqrt(m) < s + 0x1.08p-23 */

		/* compute nearest rounded result.  */
		const uint32_t d0 = (m << 16) - s * s;
		const uint32_t d1 = s - d0;
		const uint32_t d2 = d1 + s + 1;
		s += d1 >> 31;
		s &= 0x007fffff;
		s |= ey;
		return priv::BitCastU32toFloat(s);
	}

	double RKIT_CORELIB_API SoftwareSqrt(double x)
	{
		/* special case handling.  */
		uint64_t ix = priv::BitCastDoubleToU64(x);
		uint64_t top = ix >> 52;
		if (top - 0x001 >= 0x7ff - 0x001) {
			/* x < 0x1p-1022 or inf or nan.  */
			if (ix * 2 == 0)
				return x;
			if (ix == 0x7ff0000000000000)
				return x;
			if (ix > 0x7ff0000000000000)
				return (x - x) / (x - x);
			/* x is subnormal, normalize it.  */
			ix = priv::BitCastDoubleToU64(x * 0x1p52);
			top = ix >> 52;
			top -= 52;
		}

		/* argument reduction:
		   x = 4^e m; with integer e, and m in [1, 4)
		   m: fixed point representation [2.62]
		   2^e is the exponent part of the result.  */
		const int even = top & 1;
		uint64_t m = (ix << 11) | 0x8000000000000000;
		if (even) m >>= 1;
		top = (top + 0x3ff) >> 1;

		/* approximate r ~ 1/sqrt(m) and s ~ sqrt(m) when m in [1,4)

		   initial estimate:
		   7bit table lookup (1bit exponent and 6bit significand).

		   iterative approximation:
		   using 2 goldschmidt iterations with 32bit int arithmetics
		   and a final iteration with 64bit int arithmetics.

		   details:

		   the relative error (e = r0 sqrt(m)-1) of a linear estimate
		   (r0 = a m + b) is |e| < 0.085955 ~ 0x1.6p-4 at best,
		   a table lookup is faster and needs one less iteration
		   6 bit lookup table (128b) gives |e| < 0x1.f9p-8
		   7 bit lookup table (256b) gives |e| < 0x1.fdp-9
		   for single and double prec 6bit is enough but for quad
		   prec 7bit is needed (or modified iterations). to avoid
		   one more iteration >=13bit table would be needed (16k).

		   a newton-raphson iteration for r is
			 w = r*r
			 u = 3 - m*w
			 r = r*u/2
		   can use a goldschmidt iteration for s at the end or
			 s = m*r

		   first goldschmidt iteration is
			 s = m*r
			 u = 3 - s*r
			 r = r*u/2
			 s = s*u/2
		   next goldschmidt iteration is
			 u = 3 - s*r
			 r = r*u/2
			 s = s*u/2
		   and at the end r is not computed only s.

		   they use the same amount of operations and converge at the
		   same quadratic rate, i.e. if
			 r1 sqrt(m) - 1 = e, then
			 r2 sqrt(m) - 1 = -3/2 e^2 - 1/2 e^3
		   the advantage of goldschmidt is that the mul for s and r
		   are independent (computed in parallel), however it is not
		   "self synchronizing": it only uses the input m in the
		   first iteration so rounding errors accumulate. at the end
		   or when switching to larger precision arithmetics rounding
		   errors dominate so the first iteration should be used.

		   the fixed point representations are
			 m: 2.30 r: 0.32, s: 2.30, d: 2.30, u: 2.30, three: 2.30
		   and after switching to 64 bit
			 m: 2.62 r: 0.64, s: 2.62, d: 2.62, u: 2.62, three: 2.62  */

		static const uint32_t three32 = 0xc0000000;

		const uint8_t i = static_cast<uint8_t>((ix >> 46) % 128);
		uint32_t r32 = static_cast<uint32_t>(priv::kRsqrtTab[i]) << 16;
		/* |r sqrt(m) - 1| < 0x1.fdp-9 */
		uint32_t s32 = UnsignedMultiplyInt32High(static_cast<uint32_t>(m >> 32), r32);
		/* |s/sqrt(m) - 1| < 0x1.fdp-9 */
		uint32_t d32 = UnsignedMultiplyInt32High(s32, r32);
		uint32_t u32 = three32 - d32;
		r32 = UnsignedMultiplyInt32High(r32, u32) << 1;
		/* |r sqrt(m) - 1| < 0x1.7bp-16 */
		s32 = UnsignedMultiplyInt32High(s32, u32) << 1;
		/* |s/sqrt(m) - 1| < 0x1.7bp-16 */
		d32 = UnsignedMultiplyInt32High(s32, r32);
		u32 = three32 - d32;
		r32 = UnsignedMultiplyInt32High(r32, u32) << 1;
		/* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
		uint64_t r64 = static_cast<uint64_t>(r32) << 32;
		uint64_t s64 = UnsignedMultiplyInt64High(m, r64);
		uint64_t d64 = UnsignedMultiplyInt64High(s64, r64);
		uint64_t u64 = (static_cast<uint64_t>(three32) << 32) - d64;
		s64 = UnsignedMultiplyInt64High(s64, u64);  /* repr: 3.61 */
		/* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
		s64 = (s64 - 2) >> 9; /* repr: 12.52 */
		/* -0x1.09p-52 < s - sqrt(m) < -0x1.fffcp-63 */

		/* s < sqrt(m) < s + 0x1.09p-52,
		   compute nearest rounded result:
		   the nearest result to 52 bits is either s or s+0x1p-52,
		   we can decide by comparing (2^52 s + 0.5)^2 to 2^104 m.  */
		const uint64_t d0 = (m << 42) - s64 * s64;
		const uint64_t d1 = s64 - d0;
		const uint64_t d2 = d1 + s64 + 1;
		s64 += d1 >> 63;
		s64 &= 0x000fffffffffffff;
		s64 |= top << 52;
		return priv::BitCastU64ToDouble(s64);
	}
} }

