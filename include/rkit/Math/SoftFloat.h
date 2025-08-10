#pragma once

#include "rkit/Core/CoreDefs.h"

#include <stdint.h>

/*============================================================================

Based on SoftFloat IEEE Floating-Point Arithmetic Package, Release 3e,
by John R. Hauser.

Copyright 2011, 2012, 2013, 2014 The Regents of the University of California.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
	this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions, and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
	be used to endorse or promote products derived from this software without
	specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

namespace rkit { namespace math { namespace priv
{
	enum
	{
		softfloat_flag_invalid = 1,
		softfloat_flag_underflow = 2,
		softfloat_flag_overflow = 4,
		softfloat_flag_inexact = 8,
	};

	enum
	{
		softfloat_round_near_even,
		softfloat_round_near_maxMag,
		softfloat_round_min,
		softfloat_round_max,
		softfloat_round_odd,
		softfloat_round_minMag,
	};

	enum
	{
		softfloat_tininess_afterRounding,
		softfloat_tininess_beforeRounding,
	};

	struct extFloat80M { uint64_t signif; uint16_t signExp; };
	typedef extFloat80M extFloat80_t;
	struct float16_t { uint16_t v; };
	struct float32_t { uint32_t v; };
	struct float64_t { uint64_t v; };
	struct float128_t { uint64_t v[2]; };

	struct uint128 { uint64_t v0, v64; };
	struct uint64_extra { uint64_t extra, v; };
	struct uint128_extra { uint64_t extra; struct uint128 v; };
	struct exp16_sig64 { int_fast16_t exp; uint_fast64_t sig; };
	struct exp32_sig64 { int_fast32_t exp; uint64_t sig; };
	struct exp16_sig32 { int_fast16_t exp; uint_fast32_t sig; };

	/*----------------------------------------------------------------------------
	| "Common NaN" structure, used to transfer NaN representations from one format
	| to another.
	*----------------------------------------------------------------------------*/
	struct commonNaN {
		bool sign;
		uint64_t v0, v64;
	};

	bool isNaNF32UI(uint_fast32_t v);
	bool signF32UI(uint_fast32_t v);
	int_fast16_t expF32UI(uint_fast32_t v);
	uint_fast32_t fracF32UI(uint_fast32_t v);
	uint_fast32_t packToF32UI(bool sign, uint_fast32_t exp, uint_fast32_t sig);

	bool signF64UI(uint_fast64_t v);
	int_fast16_t expF64UI(uint_fast64_t v);
	uint_fast64_t fracF64UI(uint_fast64_t a);
	uint_fast64_t packToF64UI(bool sign, uint_fast32_t exp, uint_fast64_t sig);

	float64_t f32_to_f64(float32_t a);
	extFloat80_t f32_to_extF80(float32_t a);
	bool f32_lt(float32_t a, float32_t b);

	float32_t f64_to_f32(float64_t a);
	extFloat80_t f64_to_extF80(float64_t a);

	bool signExtF80UI64(uint_fast64_t a64);
	uint_fast16_t expExtF80UI64(uint_fast64_t a64);
	uint_fast16_t packToExtF80UI64(bool sign, uint_fast16_t exp);
	extFloat80_t extF80_add(extFloat80_t, extFloat80_t);
	extFloat80_t extF80_mul(extFloat80_t, extFloat80_t);

	extFloat80_t extF80_create(uint16_t signExp, uint64_t signif);
	float32_t extF80_to_f32(extFloat80_t a);
	float64_t extF80_to_f64(extFloat80_t a);
	int_fast64_t extF80_to_i64(extFloat80_t a, uint_fast8_t roundingMode, bool exact);
	extFloat80_t extF80_roundToInt(extFloat80_t a, uint_fast8_t roundingMode, bool exact);

	void softfloat_raiseFlags(uint_fast8_t);
	void softfloat_setExceptionFlag(uint_fast8_t);

	uint_fast64_t softfloat_commonNaNToF64UI(const struct commonNaN *aPtr);
	void softfloat_f64UIToCommonNaN(uint_fast64_t uiA, struct commonNaN *zPtr);
	bool softfloat_isSigNaNF64UI(uint_fast64_t uiA);
	struct exp16_sig64 softfloat_normSubnormalF64Sig(uint_fast64_t);

	uint_fast32_t softfloat_commonNaNToF32UI(const struct commonNaN *aPtr);
	void softfloat_f32UIToCommonNaN(uint_fast32_t uiA, struct commonNaN *zPtr);
	bool softfloat_isSigNaNF32UI(uint_fast32_t uiA);
	struct exp16_sig32 softfloat_normSubnormalF32Sig(uint_fast32_t);

	void softfloat_extF80UIToCommonNaN(uint_fast16_t uiA64, uint_fast64_t uiA0, struct commonNaN *zPtr);
	bool softfloat_isSigNaNExtF80UI(uint_fast16_t uiA64, uint_fast64_t uiA0);

	struct uint128 softfloat_shiftRightJam128(uint64_t a64, uint64_t a0, uint_fast32_t dist);

	uint64_t softfloat_shiftRightJam64(uint64_t a, uint_fast32_t dist);
	uint64_t softfloat_shortShiftRightJam64(uint64_t a, uint_fast8_t dist);
	struct uint64_extra softfloat_shiftRightJam64Extra(uint64_t a, uint64_t extra, uint_fast32_t dist);
	struct uint64_extra softfloat_shortShiftRightJam64Extra(uint64_t a, uint64_t extra, uint_fast8_t dist);

	uint32_t softfloat_shiftRightJam32(uint32_t a, uint_fast16_t dist);
	float32_t softfloat_roundPackToF32(bool, int_fast16_t, uint_fast32_t);

	float64_t softfloat_roundPackToF64(bool, int_fast16_t, uint_fast64_t);
	float64_t softfloat_normRoundPackToF64(bool, int_fast16_t, uint_fast64_t);

	struct uint128 softfloat_commonNaNToExtF80UI(const struct commonNaN *aPtr);
	struct uint128 softfloat_propagateNaNExtF80UI(uint_fast16_t uiA64, uint_fast64_t uiA0, uint_fast16_t uiB64, uint_fast64_t uiB0);
	extFloat80_t softfloat_addMagsExtF80(uint_fast16_t uiA64, uint_fast64_t uiA0, uint_fast16_t uiB64, uint_fast64_t uiB0, bool signZ);
	extFloat80_t softfloat_subMagsExtF80(uint_fast16_t uiA64, uint_fast64_t uiA0, uint_fast16_t uiB64, uint_fast64_t uiB0, bool signZ);

	struct exp32_sig64 softfloat_normSubnormalExtF80Sig(uint_fast64_t);
	extFloat80_t softfloat_roundPackToExtF80(bool, int_fast32_t, uint_fast64_t, uint_fast64_t, uint_fast8_t);
	extFloat80_t softfloat_normRoundPackToExtF80(bool sign, int_fast32_t exp, uint_fast64_t sig, uint_fast64_t sigExtra, uint_fast8_t roundingPrecision);

	uint_fast8_t softfloat_countLeadingZeros32(uint32_t a);
	uint_fast8_t softfloat_countLeadingZeros64(uint64_t a);
	struct uint128 softfloat_mul64To128(uint64_t a, uint64_t b);

	struct uint128 softfloat_add128(uint64_t a64, uint64_t a0, uint64_t b64, uint64_t b0);
	struct uint128 softfloat_sub128(uint64_t a64, uint64_t a0, uint64_t b64, uint64_t b0);
	struct uint128 softfloat_shortShiftLeft128(uint64_t a64, uint64_t a0, uint_fast8_t dist);

	int_fast64_t softfloat_roundToI64(bool, uint_fast64_t, uint_fast64_t, uint_fast8_t, bool);

	static const uint16_t defaultNaNExtF80UI64 = 0xFFFF;
	static const uint64_t defaultNaNExtF80UI0 = UINT64_C(0xC000000000000000);

	static const uint_fast8_t init_detectTininess = softfloat_tininess_afterRounding;
	static const uint_fast8_t softfloat_roundingMode = softfloat_round_near_even;
	static const uint_fast8_t softfloat_detectTininess = init_detectTininess;

	static const uint64_t ui64_fromPosOverflow = UINT64_C(0xFFFFFFFFFFFFFFFF);
	static const uint64_t ui64_fromNegOverflow = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	static const uint64_t ui64_fromNaN = UINT64_C( 0xFFFFFFFFFFFFFFFF );
	static const int64_t i64_fromPosOverflow = (-INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1);
	static const int64_t i64_fromNegOverflow = (-INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1);
	static const int64_t i64_fromNaN = (-INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1);

	static const uint_fast8_t extF80_roundingPrecision = 80;
} } } // rkit::math::priv

namespace rkit { namespace math
{
	class SoftFloat64;
	class SoftFloat32;
	class SoftFloat80;

	enum class RoundingMode
	{
	};

	class SoftFloat64
	{
	public:
		SoftFloat64();
		SoftFloat64(float f);
		SoftFloat64(double f);
		SoftFloat64(const priv::float64_t &value);

		static SoftFloat64 FromBits(uint64_t bits);

		float ToSingle() const;
		double ToDouble() const;
		SoftFloat32 ToFloat32() const;
		SoftFloat80 ToFloat80() const;

	private:
		priv::float64_t m_f;
	};

	class SoftFloat32
	{
	public:
		SoftFloat32();
		SoftFloat32(float f);
		SoftFloat32(const priv::float32_t &value);

		bool operator<(const SoftFloat32 &other) const;
		bool operator<=(const SoftFloat32 &other) const;
		bool operator>(const SoftFloat32 &other) const;
		bool operator>=(const SoftFloat32 &other) const;
		bool operator==(const SoftFloat32 &other) const;
		bool operator!=(const SoftFloat32 &other) const;

		static SoftFloat32 FromBits(uint32_t bits);
		uint32_t ToBits() const;

		SoftFloat64 ToFloat64() const;
		SoftFloat80 ToFloat80() const;
		float ToSingle() const;
		double ToDouble() const;

	private:
		priv::float32_t m_f;
	};

	// Software emulated x86 extended precision
	class SoftFloat80
	{
	public:
		SoftFloat80();
		SoftFloat80(float f);
		SoftFloat80(double f);
		SoftFloat80(const priv::extFloat80_t &f);
		SoftFloat80(const SoftFloat32 &f);
		SoftFloat80(const SoftFloat64 &f);
		SoftFloat80(const SoftFloat80 &f) = default;

		SoftFloat80 operator+(const SoftFloat80 &other) const;
		SoftFloat80 operator*(const SoftFloat80 &other) const;

		bool operator<(const SoftFloat80 &other) const;
		bool operator<=(const SoftFloat80 &other) const;
		bool operator>(const SoftFloat80 &other) const;
		bool operator>=(const SoftFloat80 &other) const;
		bool operator==(const SoftFloat80 &other) const;
		bool operator!=(const SoftFloat80 &other) const;

		static SoftFloat32 FromBits(uint16_t signExp, uint64_t signif);

		SoftFloat32 ToFloat32() const;
		SoftFloat64 ToFloat64() const;
		float ToSingle() const;
		double ToDouble() const;
		int64_t ToInt64() const;

		SoftFloat80 Ceil() const;
		SoftFloat80 Floor() const;

	private:
		priv::extFloat80M m_f;
	};
} }

#include "BitOps.h"
#include "ExtendedOps.h"

// SoftFloat implementations
namespace rkit { namespace math { namespace priv
{
	inline void softfloat_raiseFlags(uint_fast8_t)
	{
	}

	inline void softfloat_setExceptionFlag(uint_fast8_t)
	{
	}

	inline uint_fast64_t softfloat_commonNaNToF64UI(const struct commonNaN *aPtr)
	{
		return
			(uint_fast64_t)aPtr->sign << 63 | UINT64_C(0x7FF8000000000000)
			| aPtr->v64 >> 12;
	}

	inline void softfloat_f64UIToCommonNaN(uint_fast64_t uiA, struct commonNaN *zPtr)
	{
		if (softfloat_isSigNaNF64UI(uiA)) {
			softfloat_raiseFlags(softfloat_flag_invalid);
		}
		zPtr->sign = uiA >> 63;
		zPtr->v64 = uiA << 12;
		zPtr->v0 = 0;
	}

	inline uint_fast32_t softfloat_commonNaNToF32UI(const struct commonNaN *aPtr)
	{
		return (uint_fast32_t)aPtr->sign << 31 | 0x7FC00000 | aPtr->v64 >> 41;
	}

	inline void softfloat_f32UIToCommonNaN(uint_fast32_t uiA, struct commonNaN *zPtr)
	{
		if (softfloat_isSigNaNF32UI(uiA)) {
			softfloat_raiseFlags(softfloat_flag_invalid);
		}
		zPtr->sign = uiA >> 31;
		zPtr->v64 = (uint_fast64_t)uiA << 41;
		zPtr->v0 = 0;
	}

	inline bool softfloat_isSigNaNF32UI(uint_fast32_t uiA)
	{
		return ((((uiA) & 0x7FC00000) == 0x7F800000) && ((uiA) & 0x003FFFFF));
	}

	inline struct exp16_sig32 softfloat_normSubnormalF32Sig(uint_fast32_t sig)
	{
		int_fast8_t shiftDist;
		struct exp16_sig32 z;

		shiftDist = softfloat_countLeadingZeros32(sig) - 8;
		z.exp = 1 - shiftDist;
		z.sig = sig << shiftDist;
		return z;
	}

	inline void softfloat_extF80UIToCommonNaN(uint_fast16_t uiA64, uint_fast64_t uiA0, struct commonNaN *zPtr)
	{
		if (softfloat_isSigNaNExtF80UI(uiA64, uiA0)) {
			softfloat_raiseFlags(softfloat_flag_invalid);
		}
		zPtr->sign = uiA64 >> 15;
		zPtr->v64 = uiA0 << 1;
		zPtr->v0 = 0;

	}

	inline bool softfloat_isSigNaNExtF80UI(uint_fast16_t uiA64, uint_fast64_t uiA0)
	{
		return ((((uiA64) & 0x7FFF) == 0x7FFF) && !((uiA0)&UINT64_C(0x4000000000000000)) && ((uiA0)&UINT64_C(0x3FFFFFFFFFFFFFFF)));
	}

	inline bool softfloat_isSigNaNF64UI(uint_fast64_t uiA)
	{
		return ((((uiA)&UINT64_C(0x7FF8000000000000)) == UINT64_C(0x7FF0000000000000)) && ((uiA)&UINT64_C(0x0007FFFFFFFFFFFF)));
	}

	inline struct uint128 softfloat_shiftRightJam128(uint64_t a64, uint64_t a0, uint_fast32_t dist)
	{
		uint_fast8_t u8NegDist;
		struct uint128 z;

		if (dist < 64) {
			u8NegDist = (0u-dist);
			z.v64 = a64 >> dist;
			z.v0 =
				a64 << (u8NegDist & 63) | a0 >> dist
				| ((uint64_t)(a0 << (u8NegDist & 63)) != 0);
		}
		else {
			z.v64 = 0;
			z.v0 =
				(dist < 127)
				? a64 >> (dist & 63)
				| (((a64 & (((uint_fast64_t)1 << (dist & 63)) - 1)) | a0)
					!= 0)
				: ((a64 | a0) != 0);
		}
		return z;

	}

	inline uint64_t softfloat_shiftRightJam64(uint64_t a, uint_fast32_t dist)
	{
		return
			(dist < 63) ? a >> dist | ((uint64_t)(a << ((0u-dist) & 63)) != 0) : (a != 0);
	}

	inline uint64_t softfloat_shortShiftRightJam64(uint64_t a, uint_fast8_t dist)
	{
		return a >> dist | ((a & (((uint_fast64_t)1 << dist) - 1)) != 0);
	}

	inline struct uint64_extra
		softfloat_shiftRightJam64Extra(
			uint64_t a, uint64_t extra, uint_fast32_t dist)
	{
		struct uint64_extra z;

		if (dist < 64) {
			z.v = a >> dist;
			z.extra = a << ((0u-dist) & 63);
		}
		else {
			z.v = 0;
			z.extra = (dist == 64) ? a : (a != 0);
		}
		z.extra |= (extra != 0);
		return z;

	}

	inline struct uint64_extra
		softfloat_shortShiftRightJam64Extra(
			uint64_t a, uint64_t extra, uint_fast8_t dist)
	{
		struct uint64_extra z;
		z.v = a >> dist;
		z.extra = a << (-dist & 63) | (extra != 0);
		return z;
	}

	inline uint32_t softfloat_shiftRightJam32(uint32_t a, uint_fast16_t dist)
	{
		return (dist < 31) ? a >> dist | ((uint32_t)(a << ((0u-dist) & 31)) != 0) : (a != 0);
	}

	inline float32_t softfloat_roundPackToF32(bool sign, int_fast16_t exp, uint_fast32_t sig)
	{
		uint_fast8_t roundingMode;
		bool roundNearEven;
		uint_fast8_t roundIncrement, roundBits;
		bool isTiny;
		uint_fast32_t uiZ;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		roundingMode = softfloat_roundingMode;
		roundNearEven = (roundingMode == softfloat_round_near_even);
		roundIncrement = 0x40;
		if (!roundNearEven && (roundingMode != softfloat_round_near_maxMag)) {
			roundIncrement =
				(roundingMode
					== (sign ? softfloat_round_min : softfloat_round_max))
				? 0x7F
				: 0;
		}
		roundBits = sig & 0x7F;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (0xFD <= (unsigned int)exp) {
			if (exp < 0) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				isTiny =
					(softfloat_detectTininess == softfloat_tininess_beforeRounding)
					|| (exp < -1) || (sig + roundIncrement < 0x80000000);
				sig = softfloat_shiftRightJam32(sig, -exp);
				exp = 0;
				roundBits = sig & 0x7F;
				if (isTiny && roundBits) {
					softfloat_raiseFlags(softfloat_flag_underflow);
				}
			}
			else if ((0xFD < exp) || (0x80000000 <= sig + roundIncrement)) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				softfloat_raiseFlags(
					softfloat_flag_overflow | softfloat_flag_inexact);
				uiZ = packToF32UI(sign, 0xFF, 0) - !roundIncrement;
				goto uiZ;
			}
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		sig = (sig + roundIncrement) >> 7;
		if (roundBits) {
			softfloat_setExceptionFlag(softfloat_flag_inexact);
			if (roundingMode == softfloat_round_odd) {
				sig |= 1;
				goto packReturn;
			}
		}
		sig &= ~(uint_fast32_t)(!(roundBits ^ 0x40) & roundNearEven);
		if (!sig) exp = 0;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	packReturn:
		uiZ = packToF32UI(sign, exp, sig);
	uiZ:
		return priv::float32_t{ uiZ };
	}

	inline float64_t softfloat_roundPackToF64(bool sign, int_fast16_t exp, uint_fast64_t sig)
	{
		uint_fast8_t roundingMode;
		bool roundNearEven;
		uint_fast16_t roundIncrement, roundBits;
		bool isTiny;
		uint_fast64_t uiZ;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		roundingMode = softfloat_roundingMode;
		roundNearEven = (roundingMode == softfloat_round_near_even);
		roundIncrement = 0x200;
		if (!roundNearEven && (roundingMode != softfloat_round_near_maxMag)) {
			roundIncrement =
				(roundingMode
					== (sign ? softfloat_round_min : softfloat_round_max))
				? 0x3FF
				: 0;
		}
		roundBits = sig & 0x3FF;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (0x7FD <= (uint16_t)exp) {
			if (exp < 0) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				isTiny =
					(softfloat_detectTininess == softfloat_tininess_beforeRounding)
					|| (exp < -1)
					|| (sig + roundIncrement < UINT64_C(0x8000000000000000));
				sig = softfloat_shiftRightJam64(sig, -exp);
				exp = 0;
				roundBits = sig & 0x3FF;
				if (isTiny && roundBits) {
					softfloat_raiseFlags(softfloat_flag_underflow);
				}
			}
			else if (
				(0x7FD < exp)
				|| (UINT64_C(0x8000000000000000) <= sig + roundIncrement)
				) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				softfloat_raiseFlags(
					softfloat_flag_overflow | softfloat_flag_inexact);
				uiZ = packToF64UI(sign, 0x7FF, 0) - !roundIncrement;
				goto uiZ;
			}
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		sig = (sig + roundIncrement) >> 10;
		if (roundBits) {
			softfloat_setExceptionFlag(softfloat_flag_inexact);

			if (roundingMode == softfloat_round_odd) {
				sig |= 1;
				goto packReturn;
			}
		}
		sig &= ~(uint_fast64_t)(!(roundBits ^ 0x200) & roundNearEven);
		if (!sig) exp = 0;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	packReturn:
		uiZ = packToF64UI(sign, exp, sig);
	uiZ:
		return float64_t{ uiZ };

	}

	inline float64_t softfloat_normRoundPackToF64(bool sign, int_fast16_t exp, uint_fast64_t sig)
	{
		int_fast8_t shiftDist;

		shiftDist = softfloat_countLeadingZeros64(sig) - 1;
		exp -= shiftDist;
		if ((10 <= shiftDist) && ((unsigned int)exp < 0x7FD)) {
			uint_fast64_t uiZ = packToF64UI(sign, sig ? exp : 0, sig << (shiftDist - 10));
			return float64_t{ uiZ };
		}
		else {
			return softfloat_roundPackToF64(sign, exp, sig << shiftDist);
		}
	}



	inline struct uint128 softfloat_commonNaNToExtF80UI(const struct commonNaN *aPtr)
	{
		struct uint128 uiZ;
		uiZ.v64 = defaultNaNExtF80UI64;
		uiZ.v0 = defaultNaNExtF80UI0;
		return uiZ;
	}

	inline struct uint128
		softfloat_propagateNaNExtF80UI(
			uint_fast16_t uiA64,
			uint_fast64_t uiA0,
			uint_fast16_t uiB64,
			uint_fast64_t uiB0
		)
	{
		struct uint128 uiZ;

		if (
			softfloat_isSigNaNExtF80UI(uiA64, uiA0)
			|| softfloat_isSigNaNExtF80UI(uiB64, uiB0)
			) {
			softfloat_raiseFlags(softfloat_flag_invalid);
		}
		uiZ.v64 = defaultNaNExtF80UI64;
		uiZ.v0 = defaultNaNExtF80UI0;
		return uiZ;

	}

	inline extFloat80_t
		softfloat_addMagsExtF80(
			uint_fast16_t uiA64,
			uint_fast64_t uiA0,
			uint_fast16_t uiB64,
			uint_fast64_t uiB0,
			bool signZ
		)
	{
		int_fast32_t expA;
		uint_fast64_t sigA;
		int_fast32_t expB;
		uint_fast64_t sigB;
		int_fast32_t expDiff;
		uint_fast16_t uiZ64;
		uint_fast64_t uiZ0, sigZ, sigZExtra;
		struct exp32_sig64 normExpSig;
		int_fast32_t expZ;
		struct uint64_extra sig64Extra;
		struct uint128 uiZ;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		expA = expExtF80UI64(uiA64);
		sigA = uiA0;
		expB = expExtF80UI64(uiB64);
		sigB = uiB0;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		expDiff = expA - expB;
		if (!expDiff) {
			if (expA == 0x7FFF) {
				if ((sigA | sigB) & UINT64_C(0x7FFFFFFFFFFFFFFF)) {
					goto propagateNaN;
				}
				uiZ64 = uiA64;
				uiZ0 = uiA0;
				goto uiZ;
			}
			sigZ = sigA + sigB;
			sigZExtra = 0;
			if (!expA) {
				normExpSig = softfloat_normSubnormalExtF80Sig(sigZ);
				expZ = normExpSig.exp + 1;
				sigZ = normExpSig.sig;
				goto roundAndPack;
			}
			expZ = expA;
			goto shiftRight1;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (expDiff < 0) {
			if (expB == 0x7FFF) {
				if (sigB & UINT64_C(0x7FFFFFFFFFFFFFFF)) goto propagateNaN;
				uiZ64 = packToExtF80UI64(signZ, 0x7FFF);
				uiZ0 = uiB0;
				goto uiZ;
			}
			expZ = expB;
			if (!expA) {
				++expDiff;
				sigZExtra = 0;
				if (!expDiff) goto newlyAligned;
			}
			sig64Extra = softfloat_shiftRightJam64Extra(sigA, 0, -expDiff);
			sigA = sig64Extra.v;
			sigZExtra = sig64Extra.extra;
		}
		else {
			if (expA == 0x7FFF) {
				if (sigA & UINT64_C(0x7FFFFFFFFFFFFFFF)) goto propagateNaN;
				uiZ64 = uiA64;
				uiZ0 = uiA0;
				goto uiZ;
			}
			expZ = expA;
			if (!expB) {
				--expDiff;
				sigZExtra = 0;
				if (!expDiff) goto newlyAligned;
			}
			sig64Extra = softfloat_shiftRightJam64Extra(sigB, 0, expDiff);
			sigB = sig64Extra.v;
			sigZExtra = sig64Extra.extra;
		}
	newlyAligned:
		sigZ = sigA + sigB;
		if (sigZ & UINT64_C(0x8000000000000000)) goto roundAndPack;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	shiftRight1:
		sig64Extra = softfloat_shortShiftRightJam64Extra(sigZ, sigZExtra, 1);
		sigZ = sig64Extra.v | UINT64_C(0x8000000000000000);
		sigZExtra = sig64Extra.extra;
		++expZ;
	roundAndPack:
		return
			softfloat_roundPackToExtF80(
				signZ, expZ, sigZ, sigZExtra, extF80_roundingPrecision);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	propagateNaN:
		uiZ = softfloat_propagateNaNExtF80UI(uiA64, uiA0, uiB64, uiB0);
		uiZ64 = static_cast<uint_fast16_t>(uiZ.v64);
		uiZ0 = uiZ.v0;
	uiZ:
		return extF80_create(uiZ64, uiZ0);

	}

	inline extFloat80_t
		softfloat_subMagsExtF80(
			uint_fast16_t uiA64,
			uint_fast64_t uiA0,
			uint_fast16_t uiB64,
			uint_fast64_t uiB0,
			bool signZ
		)
	{
		int_fast32_t expA;
		uint_fast64_t sigA;
		int_fast32_t expB;
		uint_fast64_t sigB;
		int_fast32_t expDiff;
		uint_fast16_t uiZ64;
		uint_fast64_t uiZ0;
		int_fast32_t expZ;
		uint_fast64_t sigExtra;
		struct uint128 sig128, uiZ;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		expA = expExtF80UI64(uiA64);
		sigA = uiA0;
		expB = expExtF80UI64(uiB64);
		sigB = uiB0;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		expDiff = expA - expB;
		if (0 < expDiff) goto expABigger;
		if (expDiff < 0) goto expBBigger;
		if (expA == 0x7FFF) {
			if ((sigA | sigB) & UINT64_C(0x7FFFFFFFFFFFFFFF)) {
				goto propagateNaN;
			}
			softfloat_raiseFlags(softfloat_flag_invalid);
			uiZ64 = defaultNaNExtF80UI64;
			uiZ0 = defaultNaNExtF80UI0;
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		expZ = expA;
		if (!expZ) expZ = 1;
		sigExtra = 0;
		if (sigB < sigA) goto aBigger;
		if (sigA < sigB) goto bBigger;
		uiZ64 =
			packToExtF80UI64((softfloat_roundingMode == softfloat_round_min), 0);
		uiZ0 = 0;
		goto uiZ;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	expBBigger:
		if (expB == 0x7FFF) {
			if (sigB & UINT64_C(0x7FFFFFFFFFFFFFFF)) goto propagateNaN;
			uiZ64 = packToExtF80UI64(signZ ^ 1, 0x7FFF);
			uiZ0 = UINT64_C(0x8000000000000000);
			goto uiZ;
		}
		if (!expA) {
			++expDiff;
			sigExtra = 0;
			if (!expDiff) goto newlyAlignedBBigger;
		}
		sig128 = softfloat_shiftRightJam128(sigA, 0, -expDiff);
		sigA = sig128.v64;
		sigExtra = sig128.v0;
	newlyAlignedBBigger:
		expZ = expB;
	bBigger:
		signZ = !signZ;
		sig128 = softfloat_sub128(sigB, 0, sigA, sigExtra);
		goto normRoundPack;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	expABigger:
		if (expA == 0x7FFF) {
			if (sigA & UINT64_C(0x7FFFFFFFFFFFFFFF)) goto propagateNaN;
			uiZ64 = uiA64;
			uiZ0 = uiA0;
			goto uiZ;
		}
		if (!expB) {
			--expDiff;
			sigExtra = 0;
			if (!expDiff) goto newlyAlignedABigger;
		}
		sig128 = softfloat_shiftRightJam128(sigB, 0, expDiff);
		sigB = sig128.v64;
		sigExtra = sig128.v0;
	newlyAlignedABigger:
		expZ = expA;
	aBigger:
		sig128 = softfloat_sub128(sigA, 0, sigB, sigExtra);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	normRoundPack:
		return
			softfloat_normRoundPackToExtF80(
				signZ, expZ, sig128.v64, sig128.v0, extF80_roundingPrecision);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	propagateNaN:
		uiZ = softfloat_propagateNaNExtF80UI(uiA64, uiA0, uiB64, uiB0);
		uiZ64 = static_cast<uint_fast16_t>(uiZ.v64);
		uiZ0 = uiZ.v0;
	uiZ:
		return extF80_create(uiZ64, uiZ0);
	}



	inline struct exp32_sig64 softfloat_normSubnormalExtF80Sig(uint_fast64_t sig)
	{
		int_fast8_t shiftDist;
		struct exp32_sig64 z;

		shiftDist = softfloat_countLeadingZeros64(sig);
		z.exp = -shiftDist;
		z.sig = sig << shiftDist;
		return z;

	}

	extFloat80_t
		softfloat_roundPackToExtF80(
			bool sign,
			int_fast32_t exp,
			uint_fast64_t sig,
			uint_fast64_t sigExtra,
			uint_fast8_t roundingPrecision
		)
	{
		uint_fast8_t roundingMode;
		bool roundNearEven;
		uint_fast64_t roundIncrement, roundMask, roundBits;
		bool isTiny, doIncrement;
		struct uint64_extra sig64Extra;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		roundingMode = softfloat_roundingMode;
		roundNearEven = (roundingMode == softfloat_round_near_even);
		if (roundingPrecision == 80) goto precision80;
		if (roundingPrecision == 64) {
			roundIncrement = UINT64_C(0x0000000000000400);
			roundMask = UINT64_C(0x00000000000007FF);
		}
		else if (roundingPrecision == 32) {
			roundIncrement = UINT64_C(0x0000008000000000);
			roundMask = UINT64_C(0x000000FFFFFFFFFF);
		}
		else {
			goto precision80;
		}
		sig |= (sigExtra != 0);
		if (!roundNearEven && (roundingMode != softfloat_round_near_maxMag)) {
			roundIncrement =
				(roundingMode
					== (sign ? softfloat_round_min : softfloat_round_max))
				? roundMask
				: 0;
		}
		roundBits = sig & roundMask;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (0x7FFD <= (uint32_t)(exp - 1)) {
			if (exp <= 0) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				isTiny =
					(softfloat_detectTininess
						== softfloat_tininess_beforeRounding)
					|| (exp < 0)
					|| (sig <= (uint64_t)(sig + roundIncrement));
				sig = softfloat_shiftRightJam64(sig, 1 - exp);
				roundBits = sig & roundMask;
				if (roundBits) {
					if (isTiny) softfloat_raiseFlags(softfloat_flag_underflow);
					softfloat_setExceptionFlag(softfloat_flag_inexact);

					if (roundingMode == softfloat_round_odd) {
						sig |= roundMask + 1;
					}
				}
				sig += roundIncrement;
				exp = ((sig & UINT64_C(0x8000000000000000)) != 0);
				roundIncrement = roundMask + 1;
				if (roundNearEven && (roundBits << 1 == roundIncrement)) {
					roundMask |= roundIncrement;
				}
				sig &= ~roundMask;
				goto packReturn;
			}
			if (
				(0x7FFE < exp)
				|| ((exp == 0x7FFE) && ((uint64_t)(sig + roundIncrement) < sig))
				) {
				goto overflow;
			}
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (roundBits) {
			softfloat_setExceptionFlag(softfloat_flag_inexact);

			if (roundingMode == softfloat_round_odd) {
				sig = (sig & ~roundMask) | (roundMask + 1);
				goto packReturn;
			}
		}
		sig = (uint64_t)(sig + roundIncrement);
		if (sig < roundIncrement) {
			++exp;
			sig = UINT64_C(0x8000000000000000);
		}
		roundIncrement = roundMask + 1;
		if (roundNearEven && (roundBits << 1 == roundIncrement)) {
			roundMask |= roundIncrement;
		}
		sig &= ~roundMask;
		goto packReturn;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	precision80:
		doIncrement = (UINT64_C(0x8000000000000000) <= sigExtra);
		if (!roundNearEven && (roundingMode != softfloat_round_near_maxMag)) {
			doIncrement =
				(roundingMode
					== (sign ? softfloat_round_min : softfloat_round_max))
				&& sigExtra;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (0x7FFD <= (uint32_t)(exp - 1)) {
			if (exp <= 0) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				isTiny =
					(softfloat_detectTininess
						== softfloat_tininess_beforeRounding)
					|| (exp < 0)
					|| !doIncrement
					|| (sig < UINT64_C(0xFFFFFFFFFFFFFFFF));
				sig64Extra =
					softfloat_shiftRightJam64Extra(sig, sigExtra, 1 - exp);
				exp = 0;
				sig = sig64Extra.v;
				sigExtra = sig64Extra.extra;
				if (sigExtra) {
					if (isTiny) softfloat_raiseFlags(softfloat_flag_underflow);
					softfloat_setExceptionFlag(softfloat_flag_inexact);

					if (roundingMode == softfloat_round_odd) {
						sig |= 1;
						goto packReturn;
					}
				}
				doIncrement = (UINT64_C(0x8000000000000000) <= sigExtra);
				if (
					!roundNearEven
					&& (roundingMode != softfloat_round_near_maxMag)
					) {
					doIncrement =
						(roundingMode
							== (sign ? softfloat_round_min : softfloat_round_max))
						&& sigExtra;
				}
				if (doIncrement) {
					++sig;
					sig &=
						~(uint_fast64_t)
						(!(sigExtra & UINT64_C(0x7FFFFFFFFFFFFFFF))
							& roundNearEven);
					exp = ((sig & UINT64_C(0x8000000000000000)) != 0);
				}
				goto packReturn;
			}
			if (
				(0x7FFE < exp)
				|| ((exp == 0x7FFE) && (sig == UINT64_C(0xFFFFFFFFFFFFFFFF))
					&& doIncrement)
				) {
				/*----------------------------------------------------------------
				*----------------------------------------------------------------*/
				roundMask = 0;
			overflow:
				softfloat_raiseFlags(
					softfloat_flag_overflow | softfloat_flag_inexact);
				if (
					roundNearEven
					|| (roundingMode == softfloat_round_near_maxMag)
					|| (roundingMode
						== (sign ? softfloat_round_min : softfloat_round_max))
					) {
					exp = 0x7FFF;
					sig = UINT64_C(0x8000000000000000);
				}
				else {
					exp = 0x7FFE;
					sig = ~roundMask;
				}
				goto packReturn;
			}
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (sigExtra) {
			softfloat_setExceptionFlag(softfloat_flag_inexact);

			if (roundingMode == softfloat_round_odd) {
				sig |= 1;
				goto packReturn;
			}
		}
		if (doIncrement) {
			++sig;
			if (!sig) {
				++exp;
				sig = UINT64_C(0x8000000000000000);
			}
			else {
				sig &=
					~(uint_fast64_t)
					(!(sigExtra & UINT64_C(0x7FFFFFFFFFFFFFFF))
						& roundNearEven);
			}
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	packReturn:
		return extF80_create(packToExtF80UI64(sign, exp), sig);

	}

	inline extFloat80_t
		softfloat_normRoundPackToExtF80(
			bool sign,
			int_fast32_t exp,
			uint_fast64_t sig,
			uint_fast64_t sigExtra,
			uint_fast8_t roundingPrecision
		)
	{
		int_fast8_t shiftDist;
		struct uint128 sig128;

		if (!sig) {
			exp -= 64;
			sig = sigExtra;
			sigExtra = 0;
		}
		shiftDist = softfloat_countLeadingZeros64(sig);
		exp -= shiftDist;
		if (shiftDist) {
			sig128 = softfloat_shortShiftLeft128(sig, sigExtra, shiftDist);
			sig = sig128.v64;
			sigExtra = sig128.v0;
		}
		return
			softfloat_roundPackToExtF80(
				sign, exp, sig, sigExtra, roundingPrecision);

	}


	inline struct exp16_sig64 softfloat_normSubnormalF64Sig(uint_fast64_t sig)
	{
		int_fast8_t shiftDist;
		struct exp16_sig64 z;

		shiftDist = softfloat_countLeadingZeros64(sig) - 11;
		z.exp = 1 - shiftDist;
		z.sig = sig << shiftDist;
		return z;
	}

	inline uint_fast8_t softfloat_countLeadingZeros64(uint64_t a)
	{
		return 63 - rkit::bitops::FindHighestSet(a);
	}

	inline uint_fast8_t softfloat_countLeadingZeros32(uint32_t a)
	{
		return 31 - rkit::bitops::FindHighestSet(a);
	}

	inline struct uint128 softfloat_mul64To128(uint64_t a, uint64_t b)
	{
		uint128 result = {};
		rkit::extops::XMulU64(a, b, result.v0, result.v64);
		return result;
	}

	inline struct uint128 softfloat_add128(uint64_t a64, uint64_t a0, uint64_t b64, uint64_t b0)
	{
		struct uint128 z;
		z.v0 = a0 + b0;
		z.v64 = a64 + b64 + (z.v0 < a0);
		return z;
	}

	inline struct uint128 softfloat_sub128(uint64_t a64, uint64_t a0, uint64_t b64, uint64_t b0)
	{
		struct uint128 z;

		z.v0 = a0 - b0;
		z.v64 = a64 - b64 - (a0 < b0);
		return z;
	}

	inline struct uint128 softfloat_shortShiftLeft128(uint64_t a64, uint64_t a0, uint_fast8_t dist)
	{
		struct uint128 z;

		z.v64 = a64 << dist | a0 >> (-dist & 63);
		z.v0 = a0 << dist;
		return z;

	}

	inline int_fast64_t
		softfloat_roundToI64(
			bool sign,
			uint_fast64_t sig,
			uint_fast64_t sigExtra,
			uint_fast8_t roundingMode,
			bool exact
		)
	{
		int_fast64_t z;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (
			(roundingMode == softfloat_round_near_maxMag)
			|| (roundingMode == softfloat_round_near_even)
			) {
			if (UINT64_C(0x8000000000000000) <= sigExtra) goto increment;
		}
		else {
			if (
				sigExtra
				&& (sign
					? (roundingMode == softfloat_round_min)
					|| (roundingMode == softfloat_round_odd)
					: (roundingMode == softfloat_round_max))
				) {
			increment:
				++sig;
				if (!sig) goto invalid;
				if (
					(sigExtra == UINT64_C(0x8000000000000000))
					&& (roundingMode == softfloat_round_near_even)
					) {
					sig &= ~(uint_fast64_t)1;
				}
			}
		}

		z = static_cast<int64_t>(static_cast<uint64_t>(sign ? (0u - sig) : sig));
		if (z && ((z < 0) ^ sign)) goto invalid;
		if (sigExtra) {
			if (roundingMode == softfloat_round_odd) z |= 1;
			if (exact) softfloat_setExceptionFlag(softfloat_flag_inexact);
		}
		return z;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	invalid:
		softfloat_raiseFlags(softfloat_flag_invalid);
		return sign ? i64_fromNegOverflow : i64_fromPosOverflow;

	}




	inline bool isNaNF32UI(uint_fast32_t a)
	{
		return (((~(a) & 0x7F800000) == 0) && ((a) & 0x007FFFFF));
	}

	inline bool signF32UI(uint_fast32_t a)
	{
		return ((bool)((uint32_t)(a) >> 31));
	}

	inline int_fast16_t expF32UI(uint_fast32_t a)
	{
		return ((int_fast16_t)((a) >> 23) & 0xFF);
	}

	inline uint_fast32_t fracF32UI(uint_fast32_t a)
	{
		return ((a) & 0x007FFFFF);
	}

	inline uint_fast32_t packToF32UI(bool sign, uint_fast32_t exp, uint_fast32_t sig)
	{
		return (((uint32_t)(sign) << 31) + ((uint32_t)(exp) << 23) + (sig));
	}

	inline bool signF64UI(uint_fast64_t a)
	{
		return ((bool)((uint64_t)(a) >> 63));
	}

	inline int_fast16_t expF64UI(uint_fast64_t a)
	{
		return ((int_fast16_t)((a) >> 52) & 0x7FF);
	}

	inline uint_fast64_t fracF64UI(uint_fast64_t a)
	{
		return ((a)&UINT64_C(0x000FFFFFFFFFFFFF));
	}

	inline uint_fast64_t packToF64UI(bool sign, uint_fast32_t exp, uint_fast64_t sig)
	{
		return ((uint64_t)(((uint_fast64_t)(sign) << 63) + ((uint_fast64_t)(exp) << 52) + (sig)));
	}

	inline bool signExtF80UI64(uint_fast64_t a64)
	{
		return ((bool)((uint16_t)(a64) >> 15));
	}

	inline uint_fast16_t expExtF80UI64(uint_fast64_t a64)
	{
		return ((a64) & 0x7FFF);
	}

	inline uint_fast16_t packToExtF80UI64(bool sign, uint_fast16_t exp)
	{
		return ((uint_fast16_t)(sign) << 15 | (exp));
	}

	inline extFloat80_t extF80_add(extFloat80_t a, extFloat80_t b)
	{
		uint_fast16_t uiA64;
		uint_fast64_t uiA0;
		bool signA;
		uint_fast16_t uiB64;
		uint_fast64_t uiB0;
		bool signB;

		uiA64 = a.signExp;
		uiA0 = a.signif;
		signA = signExtF80UI64(uiA64);
		uiB64 = b.signExp;
		uiB0 = b.signif;
		signB = signExtF80UI64(uiB64);

		if (signA == signB) {
			return softfloat_addMagsExtF80(uiA64, uiA0, uiB64, uiB0, signA);
		}
		else {
			return softfloat_subMagsExtF80(uiA64, uiA0, uiB64, uiB0, signA);
		}

	}

	inline extFloat80_t extF80_mul(extFloat80_t a, extFloat80_t b)
	{
		uint_fast16_t uiA64;
		uint_fast64_t uiA0;
		bool signA;
		int_fast32_t expA;
		uint_fast64_t sigA;
		uint_fast16_t uiB64;
		uint_fast64_t uiB0;
		bool signB;
		int_fast32_t expB;
		uint_fast64_t sigB;
		bool signZ;
		uint_fast64_t magBits;
		struct exp32_sig64 normExpSig;
		int_fast32_t expZ;
		struct uint128 sig128Z, uiZ;
		uint_fast16_t uiZ64;
		uint_fast64_t uiZ0;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA64 = a.signExp;
		uiA0 = a.signif;
		signA = signExtF80UI64(uiA64);
		expA = expExtF80UI64(uiA64);
		sigA = uiA0;
		uiB64 = b.signExp;
		uiB0 = b.signif;
		signB = signExtF80UI64(uiB64);
		expB = expExtF80UI64(uiB64);
		sigB = uiB0;
		signZ = signA ^ signB;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (expA == 0x7FFF) {
			if (
				(sigA & UINT64_C(0x7FFFFFFFFFFFFFFF))
				|| ((expB == 0x7FFF) && (sigB & UINT64_C(0x7FFFFFFFFFFFFFFF)))
				) {
				goto propagateNaN;
			}
			magBits = expB | sigB;
			goto infArg;
		}
		if (expB == 0x7FFF) {
			if (sigB & UINT64_C(0x7FFFFFFFFFFFFFFF)) goto propagateNaN;
			magBits = expA | sigA;
			goto infArg;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (!expA) expA = 1;
		if (!(sigA & UINT64_C(0x8000000000000000))) {
			if (!sigA) goto zero;
			normExpSig = softfloat_normSubnormalExtF80Sig(sigA);
			expA += normExpSig.exp;
			sigA = normExpSig.sig;
		}
		if (!expB) expB = 1;
		if (!(sigB & UINT64_C(0x8000000000000000))) {
			if (!sigB) goto zero;
			normExpSig = softfloat_normSubnormalExtF80Sig(sigB);
			expB += normExpSig.exp;
			sigB = normExpSig.sig;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		expZ = expA + expB - 0x3FFE;
		sig128Z = softfloat_mul64To128(sigA, sigB);
		if (sig128Z.v64 < UINT64_C(0x8000000000000000)) {
			--expZ;
			sig128Z =
				softfloat_add128(
					sig128Z.v64, sig128Z.v0, sig128Z.v64, sig128Z.v0);
		}
		return
			softfloat_roundPackToExtF80(
				signZ, expZ, sig128Z.v64, sig128Z.v0, extF80_roundingPrecision);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	propagateNaN:
		uiZ = softfloat_propagateNaNExtF80UI(uiA64, uiA0, uiB64, uiB0);
		uiZ64 = static_cast<uint_fast16_t>(uiZ.v64);
		uiZ0 = uiZ.v0;
		goto uiZ;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	infArg:
		if (!magBits) {
			softfloat_raiseFlags(softfloat_flag_invalid);
			uiZ64 = defaultNaNExtF80UI64;
			uiZ0 = defaultNaNExtF80UI0;
		}
		else {
			uiZ64 = packToExtF80UI64(signZ, 0x7FFF);
			uiZ0 = UINT64_C(0x8000000000000000);
		}
		goto uiZ;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	zero:
		uiZ64 = packToExtF80UI64(signZ, 0);
		uiZ0 = 0;
	uiZ:
		return extF80_create(uiZ64, uiZ0);

	}

	inline float64_t f32_to_f64(float32_t a)
	{
		uint_fast32_t uiA;
		bool sign;
		int_fast16_t exp;
		uint_fast32_t frac;
		struct commonNaN commonNaN;
		uint_fast64_t uiZ;
		struct exp16_sig32 normExpSig;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA = a.v;
		sign = signF32UI(uiA);
		exp = expF32UI(uiA);
		frac = fracF32UI(uiA);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (exp == 0xFF) {
			if (frac) {
				softfloat_f32UIToCommonNaN(uiA, &commonNaN);
				uiZ = softfloat_commonNaNToF64UI(&commonNaN);
			}
			else {
				uiZ = packToF64UI(sign, 0x7FF, 0);
			}
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (!exp) {
			if (!frac) {
				uiZ = packToF64UI(sign, 0, 0);
				goto uiZ;
			}
			normExpSig = softfloat_normSubnormalF32Sig(frac);
			exp = normExpSig.exp - 1;
			frac = normExpSig.sig;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiZ = packToF64UI(sign, exp + 0x380, (uint_fast64_t)frac << 29);
	uiZ:
		return float64_t{ uiZ };
	}

	inline extFloat80_t f32_to_extF80(float32_t a)
	{
		uint_fast32_t uiA;
		bool sign;
		int_fast16_t exp;
		uint_fast32_t frac;
		struct commonNaN commonNaN;
		struct uint128 uiZ;
		uint_fast16_t uiZ64;
		uint_fast64_t uiZ0;
		struct exp16_sig32 normExpSig;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA = a.v;
		sign = signF32UI(uiA);
		exp = expF32UI(uiA);
		frac = fracF32UI(uiA);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (exp == 0xFF) {
			if (frac) {
				softfloat_f32UIToCommonNaN(uiA, &commonNaN);
				uiZ = softfloat_commonNaNToExtF80UI(&commonNaN);
				uiZ64 = static_cast<uint_fast16_t>(uiZ.v64);
				uiZ0 = uiZ.v0;
			}
			else {
				uiZ64 = packToExtF80UI64(sign, 0x7FFF);
				uiZ0 = UINT64_C(0x8000000000000000);
			}
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (!exp) {
			if (!frac) {
				uiZ64 = packToExtF80UI64(sign, 0);
				uiZ0 = 0;
				goto uiZ;
			}
			normExpSig = softfloat_normSubnormalF32Sig(frac);
			exp = normExpSig.exp;
			frac = normExpSig.sig;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiZ64 = packToExtF80UI64(sign, exp + 0x3F80);
		uiZ0 = (uint_fast64_t)(frac | 0x00800000) << 40;
	uiZ:
		return extF80_create(uiZ64, uiZ0);

	}

	inline bool f32_lt(float32_t a, float32_t b)
	{
		uint_fast32_t uiA = a.v;
		uint_fast32_t uiB = b.v;
		bool signA, signB;

		if (isNaNF32UI(uiA) || isNaNF32UI(uiB)) {
			softfloat_raiseFlags(softfloat_flag_invalid);
			return false;
		}
		signA = signF32UI(uiA);
		signB = signF32UI(uiB);
		return
			(signA != signB) ? signA && ((uint32_t)((uiA | uiB) << 1) != 0)
			: (uiA != uiB) && (signA ^ (uiA < uiB));
	}

	inline float32_t f64_to_f32(float64_t a)
	{
		uint_fast64_t uiA;
		bool sign;
		int_fast16_t exp;
		uint_fast64_t frac;
		struct commonNaN commonNaN;
		uint_fast32_t uiZ, frac32;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA = a.v;
		sign = signF64UI(uiA);
		exp = expF64UI(uiA);
		frac = fracF64UI(uiA);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (exp == 0x7FF) {
			if (frac) {
				softfloat_f64UIToCommonNaN(uiA, &commonNaN);
				uiZ = softfloat_commonNaNToF32UI(&commonNaN);
			}
			else {
				uiZ = packToF32UI(sign, 0xFF, 0);
			}
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		frac32 = (uint_fast32_t)softfloat_shortShiftRightJam64(frac, 22);
		if (!(exp | frac32)) {
			uiZ = packToF32UI(sign, 0, 0);
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		return softfloat_roundPackToF32(sign, exp - 0x381, frac32 | 0x40000000);
	uiZ:
		return priv::float32_t{ uiZ };
	}

	extFloat80_t f64_to_extF80(float64_t a)
	{
		uint_fast64_t uiA;
		bool sign;
		int_fast16_t exp;
		uint_fast64_t frac;
		struct commonNaN commonNaN;
		struct uint128 uiZ;
		uint_fast16_t uiZ64;
		uint_fast64_t uiZ0;
		struct exp16_sig64 normExpSig;
		union { struct extFloat80M s; extFloat80_t f; } uZ;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA = a.v;
		sign = signF64UI(uiA);
		exp = expF64UI(uiA);
		frac = fracF64UI(uiA);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (exp == 0x7FF) {
			if (frac) {
				softfloat_f64UIToCommonNaN(uiA, &commonNaN);
				uiZ = softfloat_commonNaNToExtF80UI(&commonNaN);
				uiZ64 = static_cast<uint_fast16_t>(uiZ.v64);
				uiZ0 = uiZ.v0;
			}
			else {
				uiZ64 = packToExtF80UI64(sign, 0x7FFF);
				uiZ0 = UINT64_C(0x8000000000000000);
			}
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (!exp) {
			if (!frac) {
				uiZ64 = packToExtF80UI64(sign, 0);
				uiZ0 = 0;
				goto uiZ;
			}
			normExpSig = softfloat_normSubnormalF64Sig(frac);
			exp = normExpSig.exp;
			frac = normExpSig.sig;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiZ64 = packToExtF80UI64(sign, exp + 0x3C00);
		uiZ0 = (frac | UINT64_C(0x0010000000000000)) << 11;
	uiZ:
		uZ.s.signExp = uiZ64;
		uZ.s.signif = uiZ0;
		return uZ.f;

	}


	inline extFloat80_t extF80_create(uint16_t signExp, uint64_t signif)
	{
		extFloat80_t result = {};
		result.signExp = signExp;
		result.signif = signif;
		return result;
	}

	inline float32_t extF80_to_f32(extFloat80_t a)
	{
		uint_fast16_t uiA64;
		uint_fast64_t uiA0;
		bool sign;
		int_fast32_t exp;
		uint_fast64_t sig;
		struct commonNaN commonNaN;
		uint_fast32_t uiZ, sig32;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA64 = a.signExp;
		uiA0 = a.signif;
		sign = signExtF80UI64(uiA64);
		exp = expExtF80UI64(uiA64);
		sig = uiA0;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (exp == 0x7FFF) {
			if (sig & UINT64_C(0x7FFFFFFFFFFFFFFF)) {
				softfloat_extF80UIToCommonNaN(uiA64, uiA0, &commonNaN);
				uiZ = softfloat_commonNaNToF32UI(&commonNaN);
			}
			else {
				uiZ = packToF32UI(sign, 0xFF, 0);
			}
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		sig32 = static_cast<uint_fast32_t>(softfloat_shortShiftRightJam64(sig, 33));
		if (!(exp | sig32)) {
			uiZ = packToF32UI(sign, 0, 0);
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		exp -= 0x3F81;
		if (sizeof(int_fast16_t) < sizeof(int_fast32_t)) {
			if (exp < -0x1000) exp = -0x1000;
		}
		return softfloat_roundPackToF32(sign, exp, sig32);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	uiZ:
		return float32_t{ uiZ };
	}

	inline float64_t extF80_to_f64(extFloat80_t a)
	{
		uint_fast16_t uiA64;
		uint_fast64_t uiA0;
		bool sign;
		int_fast32_t exp;
		uint_fast64_t sig;
		struct commonNaN commonNaN;
		uint_fast64_t uiZ;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA64 = a.signExp;
		uiA0 = a.signif;
		sign = signExtF80UI64(uiA64);
		exp = expExtF80UI64(uiA64);
		sig = uiA0;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (!(exp | sig)) {
			uiZ = packToF64UI(sign, 0, 0);
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (exp == 0x7FFF) {
			if (sig & UINT64_C(0x7FFFFFFFFFFFFFFF)) {
				softfloat_extF80UIToCommonNaN(uiA64, uiA0, &commonNaN);
				uiZ = softfloat_commonNaNToF64UI(&commonNaN);
			}
			else {
				uiZ = packToF64UI(sign, 0x7FF, 0);
			}
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		sig = softfloat_shortShiftRightJam64(sig, 1);
		exp -= 0x3C01;
		if (sizeof(int_fast16_t) < sizeof(int_fast32_t)) {
			if (exp < -0x1000) exp = -0x1000;
		}
		return softfloat_roundPackToF64(sign, exp, sig);
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
	uiZ:
		return float64_t{ uiZ };

	}

	inline int_fast64_t extF80_to_i64(extFloat80_t a, uint_fast8_t roundingMode, bool exact)
	{
		uint_fast16_t uiA64;
		bool sign;
		int_fast32_t exp;
		uint_fast64_t sig;
		int_fast32_t shiftDist;
		uint_fast64_t sigExtra;
		struct uint64_extra sig64Extra;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA64 = a.signExp;
		sign = signExtF80UI64(uiA64);
		exp = expExtF80UI64(uiA64);
		sig = a.signif;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		shiftDist = 0x403E - exp;
		if (shiftDist <= 0) {
			/*--------------------------------------------------------------------
			*--------------------------------------------------------------------*/
			if (shiftDist) {
				softfloat_raiseFlags(softfloat_flag_invalid);
				return
					(exp == 0x7FFF) && (sig & UINT64_C(0x7FFFFFFFFFFFFFFF))
					? i64_fromNaN
					: sign ? i64_fromNegOverflow : i64_fromPosOverflow;
			}
			/*--------------------------------------------------------------------
			*--------------------------------------------------------------------*/
			sigExtra = 0;
		}
		else {
			/*--------------------------------------------------------------------
			*--------------------------------------------------------------------*/
			sig64Extra = softfloat_shiftRightJam64Extra(sig, 0, shiftDist);
			sig = sig64Extra.v;
			sigExtra = sig64Extra.extra;
		}
		return softfloat_roundToI64(sign, sig, sigExtra, roundingMode, exact);

	}

	inline extFloat80_t extF80_roundToInt(extFloat80_t a, uint_fast8_t roundingMode, bool exact)
	{
		uint_fast16_t uiA64, signUI64;
		int_fast32_t exp;
		uint_fast64_t sigA;
		uint_fast16_t uiZ64;
		uint_fast64_t sigZ;
		struct exp32_sig64 normExpSig;
		struct uint128 uiZ;
		uint_fast64_t lastBitMask, roundBitsMask;

		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiA64 = a.signExp;
		signUI64 = uiA64 & packToExtF80UI64(1, 0);
		exp = expExtF80UI64(uiA64);
		sigA = a.signif;
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (!(sigA & UINT64_C(0x8000000000000000)) && (exp != 0x7FFF)) {
			if (!sigA) {
				uiZ64 = signUI64;
				sigZ = 0;
				goto uiZ;
			}
			normExpSig = softfloat_normSubnormalExtF80Sig(sigA);
			exp += normExpSig.exp;
			sigA = normExpSig.sig;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		if (0x403E <= exp) {
			if (exp == 0x7FFF) {
				if (sigA & UINT64_C(0x7FFFFFFFFFFFFFFF)) {
					uiZ = softfloat_propagateNaNExtF80UI(uiA64, sigA, 0, 0);
					uiZ64 = static_cast<uint_fast16_t>(uiZ.v64);
					sigZ = uiZ.v0;
					goto uiZ;
				}
				sigZ = UINT64_C(0x8000000000000000);
			}
			else {
				sigZ = sigA;
			}
			uiZ64 = signUI64 | exp;
			goto uiZ;
		}
		if (exp <= 0x3FFE) {
			if (exact) softfloat_setExceptionFlag(softfloat_flag_inexact);
			switch (roundingMode) {
			case softfloat_round_near_even:
				if (!(sigA & UINT64_C(0x7FFFFFFFFFFFFFFF))) break;
			case softfloat_round_near_maxMag:
				if (exp == 0x3FFE) goto mag1;
				break;
			case softfloat_round_min:
				if (signUI64) goto mag1;
				break;
			case softfloat_round_max:
				if (!signUI64) goto mag1;
				break;
			case softfloat_round_odd:
				goto mag1;
			}
			uiZ64 = signUI64;
			sigZ = 0;
			goto uiZ;
		mag1:
			uiZ64 = signUI64 | 0x3FFF;
			sigZ = UINT64_C(0x8000000000000000);
			goto uiZ;
		}
		/*------------------------------------------------------------------------
		*------------------------------------------------------------------------*/
		uiZ64 = signUI64 | exp;
		lastBitMask = (uint_fast64_t)1 << (0x403E - exp);
		roundBitsMask = lastBitMask - 1;
		sigZ = sigA;
		if (roundingMode == softfloat_round_near_maxMag) {
			sigZ += lastBitMask >> 1;
		}
		else if (roundingMode == softfloat_round_near_even) {
			sigZ += lastBitMask >> 1;
			if (!(sigZ & roundBitsMask)) sigZ &= ~lastBitMask;
		}
		else if (
			roundingMode == (signUI64 ? softfloat_round_min : softfloat_round_max)
			) {
			sigZ += roundBitsMask;
		}
		sigZ &= ~roundBitsMask;
		if (!sigZ) {
			++uiZ64;
			sigZ = UINT64_C(0x8000000000000000);
		}
		if (sigZ != sigA) {
			if (roundingMode == softfloat_round_odd) sigZ |= lastBitMask;
			if (exact) softfloat_setExceptionFlag(softfloat_flag_inexact);
		}
	uiZ:
		return extF80_create(uiZ64, sigZ);

	}



} } } // rkit::math::priv

#include <string.h>

namespace rkit { namespace math
{
	inline SoftFloat64::SoftFloat64()
		: m_f{ 0 }
	{
	}

	inline SoftFloat64::SoftFloat64(float f)
		: m_f{ 0 }
	{
		(*this) = SoftFloat32(f).ToFloat64();
	}

	inline SoftFloat64::SoftFloat64(double f)
		: m_f{ 0 }
	{
		memcpy(&m_f.v, &f, 8);
	}

	inline SoftFloat64::SoftFloat64(const priv::float64_t &value)
		: m_f(value)
	{
	}

	inline SoftFloat64 SoftFloat64::FromBits(uint64_t bits)
	{
		const priv::float64_t value = { bits };
		return SoftFloat64(value);
	}

	inline float SoftFloat64::ToSingle() const
	{
		return ToFloat32().ToSingle();
	}

	inline double SoftFloat64::ToDouble() const
	{
		double result;
		memcpy(&result, &m_f.v, 8);
		return result;
	}

	SoftFloat32 SoftFloat64::ToFloat32() const
	{
		return SoftFloat32(priv::f64_to_f32(m_f));
	}

	SoftFloat80 SoftFloat64::ToFloat80() const
	{
		return SoftFloat80(priv::f64_to_extF80(m_f));
	}

	inline SoftFloat32::SoftFloat32()
		: m_f{ 0 }
	{
	}

	inline SoftFloat32::SoftFloat32(float f)
		: m_f{ 0 }
	{
		memcpy(&m_f.v, &f, 4);
	}

	inline SoftFloat32::SoftFloat32(const priv::float32_t &value)
		: m_f(value)
	{
	}

	inline bool SoftFloat32::operator<(const SoftFloat32 &other) const
	{
		return f32_lt(m_f, other.m_f);
	}

	inline bool SoftFloat32::operator>(const SoftFloat32 &other) const
	{
		return f32_lt(other.m_f, m_f);
	}

	inline SoftFloat32 SoftFloat32::FromBits(uint32_t bits)
	{
		const priv::float32_t value = { bits };
		return SoftFloat32(value);
	}

	uint32_t SoftFloat32::ToBits() const
	{
		return m_f.v;
	}

	SoftFloat64 SoftFloat32::ToFloat64() const
	{
		return SoftFloat64(priv::f32_to_f64(m_f));
	}

	SoftFloat80 SoftFloat32::ToFloat80() const
	{
		return SoftFloat80(priv::f32_to_extF80(m_f));
	}

	inline float SoftFloat32::ToSingle() const
	{
		float result;
		memcpy(&result, &m_f.v, 4);
		return result;
	}

	inline double SoftFloat32::ToDouble() const
	{
		return ToFloat64().ToDouble();
	}

	inline SoftFloat80::SoftFloat80()
		: m_f{ 0, 0 }
	{
	}

	inline SoftFloat80::SoftFloat80(float f)
		: m_f{ 0, 0 }
	{
		(*this) = SoftFloat32(f).ToFloat80();
	}

	inline SoftFloat80::SoftFloat80(double f)
		: m_f{ 0, 0 }
	{
		(*this) = SoftFloat64(f).ToFloat80();
	}

	inline SoftFloat80::SoftFloat80(const priv::extFloat80_t &f)
		: m_f(f)
	{
	}

	inline SoftFloat80::SoftFloat80(const SoftFloat32 &f)
		: m_f{ 0, 0 }
	{
		(*this) = f.ToFloat80();
	}

	inline SoftFloat80::SoftFloat80(const SoftFloat64 &f)
		: m_f{ 0, 0 }
	{
		(*this) = f.ToFloat80();
	}

	inline SoftFloat80 SoftFloat80::operator+(const SoftFloat80 &other) const
	{
		return priv::extF80_add(m_f, other.m_f);
	}

	inline SoftFloat80 SoftFloat80::operator*(const SoftFloat80 &other) const
	{
		return priv::extF80_mul(m_f, other.m_f);
	}

	inline SoftFloat32 SoftFloat80::ToFloat32() const
	{
		return SoftFloat32(priv::extF80_to_f32(m_f));
	}

	inline SoftFloat64 SoftFloat80::ToFloat64() const
	{
		return SoftFloat64(priv::extF80_to_f64(m_f));
	}

	float SoftFloat80::ToSingle() const
	{
		return ToFloat32().ToSingle();
	}

	double SoftFloat80::ToDouble() const
	{
		return ToFloat64().ToDouble();
	}

	int64_t SoftFloat80::ToInt64() const
	{
		return priv::extF80_to_i64(m_f, priv::softfloat_round_minMag, false);
	}

	SoftFloat80 SoftFloat80::Ceil() const
	{
		return priv::extF80_roundToInt(m_f, priv::softfloat_round_max, false);
	}

	SoftFloat80 SoftFloat80::Floor() const
	{
		return priv::extF80_roundToInt(m_f, priv::softfloat_round_min, false);
	}

} } // rkit::math

