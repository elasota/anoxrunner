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
#include <cfloat>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <utility>
#include <cmath>

#include "rkit/Core/CoreLib.h"

#if LDBL_MANT_DIG == 53 && LDBL_MAX_EXP == 1024

#define LD_B1B_DIG 2
#define LD_B1B_MAX 9007199, 254740991
#define KMAX 128

#elif LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384

#define LD_B1B_DIG 3
#define LD_B1B_MAX 18, 446744073, 709551615
#define KMAX 2048

#elif LDBL_MANT_DIG == 113 && LDBL_MAX_EXP == 16384

#define LD_B1B_DIG 4
#define LD_B1B_MAX 10384593, 717069655, 257060992, 658440191
#define KMAX 2048

#else
#error Unsupported long double representation
#endif

#define MASK (KMAX-1)

namespace rkit { namespace corelib {
	template<class TChar>
	class MemFloatScanState
	{
	public:
		MemFloatScanState(const TChar *chars, size_t len)
			: m_chars(chars)
			, m_pos(0)
			, m_length(len)
			, m_overrun(0)
		{
		}

		std::pair<bool, int32_t> GetOne()
		{
			if (m_pos == m_length)
			{
				m_overrun++;
				return std::pair<bool, int32_t>(false, 0);
			}
			else
			{
				return std::pair<bool, int32_t>(true, static_cast<int32_t>(m_chars[m_pos++]));
			}
		}

		void UnGetOne()
		{
			if (m_overrun)
				m_overrun--;
			else
				m_pos--;
		}

		size_t GetPos() const
		{
			return m_pos;
		}

	private:
		const TChar *m_chars;
		size_t m_pos;
		size_t m_length;
		size_t m_overrun;
	};


	class ProxyScanState
	{
	public:
		typedef bool (*GetOneCharCb_t)(void *state, int32_t &outChar);
		typedef void (*UnGetCharCb_t)(void *state);

		ProxyScanState(void *state, GetOneCharCb_t getOneCharCb, UnGetCharCb_t unGetCharCb)
			: m_state(state)
			, m_getOneCharCb(getOneCharCb)
			, m_unGetCharCb(unGetCharCb)
		{
		}

		std::pair<bool, int32_t> GetOne() const
		{
			int32_t ch = 0;
			if (m_getOneCharCb(m_state, ch))
				return std::pair<bool, int32_t>(true, ch);
			else
				return std::pair<bool, int32_t>(false, 0);
		}

		void UnGetOne() const
		{
			m_unGetCharCb(m_state);
		}

	private:
		void *m_state;
		GetOneCharCb_t m_getOneCharCb;
		UnGetCharCb_t m_unGetCharCb;
	};
} }

static bool ishexchar(int32_t ch)
{
	ch |= 32;
	return ch >= 'a' && ch <= 'f';
}

static bool isdigitchar(int32_t ch)
{
	return ch >= '0' && ch <= '9';
}

static bool isdigitchar(const std::pair<bool, int32_t> &ch)
{
	return ch.first && isdigitchar(ch.second);
}

static bool isspecific(const std::pair<bool, int32_t> &ch, int32_t check)
{
	return ch.first && ch.second == check;
}

template<class TFloatScanState>
static int64_t scanexp(TFloatScanState *f, bool pok)
{
	std::pair<bool, int32_t> c;
	int32_t x;
	int64_t y;
	bool neg = false;

	c = f->GetOne();
	if (c.first && (c.second == '+' || c.second == '-'))
	{
		neg = (c.second == '-');
		c = f->GetOne();
		if (!isdigitchar(c) && pok)
		{
			f->UnGetOne();
		}
	}
	if (!isdigitchar(c.second))
	{
		f->UnGetOne();
		return std::numeric_limits<int64_t>::min();
	}
	for (x = 0; isdigitchar(c) && x < std::numeric_limits<int32_t>::max() / 10; c = f->GetOne())
	{
		x = 10 * x + c.second - '0';
	}
	for (y = x; isdigitchar(c) && y < std::numeric_limits<int64_t>::max() / 100; c = f->GetOne())
	{
		y = 10 * y + c.second - '0';
	}
	while (isdigitchar(c))
	{
		c = f->GetOne();
	}
	f->UnGetOne();
	return neg ? -y : y;
}

enum class ErrType
{
	kNone,
	kInvalid,
	kRange,
};

template<class TFloatScanState>
long double decfloat(ErrType &err, TFloatScanState *f, std::pair<bool, int32_t> c, int bits, int emin, int sign, int pok)
{
	uint32_t x[KMAX];
	static const uint32_t th[] = { LD_B1B_MAX };
	int32_t i, j, k, a, z;
	int64_t lrp = 0, dc = 0;
	int64_t e10 = 0;
	int lnz = 0;
	bool gotdig = false;
	bool gotrad = false;
	int rp;
	int e2;
	int emax = -emin - bits + 3;
	int denormal = 0;
	long double y;
	long double frac = 0;
	long double bias = 0;
	static const int p10s[] = { 10, 100, 1000, 10000,
		100000, 1000000, 10000000, 100000000 };

	j = 0;
	k = 0;

	/* Don't let leading zeros consume buffer space */
	for (; isspecific(c, '0'); c = f->GetOne())
	{
		gotdig = true;
	}
	if (isspecific(c, '.'))
	{
		gotrad = 1;
		for (c = f->GetOne(); isspecific(c, '0'); c = f->GetOne())
		{
			gotdig = true;
			lrp--;
		}
	}

	x[0] = 0;
	for (; c.first && (isdigitchar(c.second) || c.second == '.'); c = f->GetOne())
	{
		if (c.second == '.')
		{
			if (gotrad)
			{
				break;
			}
			gotrad = true;
			lrp = dc;
		}
		else if (k < KMAX - 3)
		{
			dc++;
			if (c.second != '0')
			{
				lnz = dc;
			}
			if (j)
			{
				x[k] = x[k] * 10 + (c.second - '0');
			}
			else
			{
				x[k] = c.second - '0';
			}

			if (++j == 9)
			{
				k++;
				j = 0;
			}
			gotdig = true;
		}
		else
		{
			dc++;
			if (c.second != '0')
			{
				lnz = (KMAX - 4) * 9;
				x[KMAX - 4] |= 1;
			}
		}
	}
	if (!gotrad)
	{
		lrp = dc;
	}

	if (gotdig && c.first && ((c.second | 32) == 'e'))
	{
		e10 = scanexp(f, pok);
		if (e10 == std::numeric_limits<int64_t>::min())
		{
			if (pok)
			{
				f->UnGetOne();
			}
			else
			{
				return 0;
			}
			e10 = 0;
		}
		lrp += e10;
	}
	else if (c.first)
	{
		f->UnGetOne();
	}
	if (!gotdig)
	{
		err = ErrType::kInvalid;
		return 0;
	}

	/* Handle zero specially to avoid nasty special cases later */
	if (!x[0])
	{
		return sign * 0.0;
	}

	/* Optimize small integers (w/no exponent) and over/under-flow */
	if (lrp == dc && dc < 10 && (bits > 30 || x[0] >> bits == 0))
	{
		return sign * static_cast<long double>(x[0]);
	}
	if (lrp > -emin / 2)
	{
		err = ErrType::kRange;
		return sign * std::numeric_limits<long double>::max() * std::numeric_limits<long double>::max();
	}
	if (lrp < emin - 2 * LDBL_MANT_DIG)
	{
		err = ErrType::kRange;
		return sign * std::numeric_limits<long double>::min() * std::numeric_limits<long double>::min();
	}

	/* Align incomplete final B1B digit */
	if (j)
	{
		for (; j < 9; j++)
		{
			x[k] *= 10;
		}
		k++;
		j = 0;
	}

	a = 0;
	z = k;
	e2 = 0;
	rp = lrp;

	/* Optimize small to mid-size integers (even in exp. notation) */
	if (lnz < 9 && lnz <= rp && rp < 18)
	{
		if (rp == 9) return sign * static_cast<long double>(x[0]);
		if (rp < 9) return sign * static_cast<long double>(x[0]) / p10s[8 - rp];
		int bitlim = bits - 3 * static_cast<int>(rp - 9);
		if (bitlim > 30 || x[0] >> bitlim == 0)
		{
			return sign * static_cast<long double>(x[0]) * p10s[rp - 10];
		}
	}

	/* Drop trailing zeros */
	for (; !x[z - 1]; z--);

	/* Align radix point to B1B digit boundary */
	if (rp % 9)
	{
		int rpm9 = rp >= 0 ? rp % 9 : rp % 9 + 9;
		int p10 = p10s[8 - rpm9];
		uint32_t carry = 0;
		for (k = a; k != z; k++) {
			uint32_t tmp = x[k] % p10;
			x[k] = x[k] / p10 + carry;
			carry = 1000000000 / p10 * tmp;
			if (k == a && !x[k])
			{
				a = ((a + 1) & MASK);
				rp -= 9;
			}
		}
		if (carry)
		{
			x[z++] = carry;
		}
		rp += 9 - rpm9;
	}

	/* Upscale until desired number of bits are left of radix point */
	while (rp < 9 * LD_B1B_DIG || (rp == 9 * LD_B1B_DIG && x[a] < th[0]))
	{
		uint32_t carry = 0;
		e2 -= 29;
		for (k = (z - 1 & MASK); ; k = (k - 1 & MASK)) {
			uint64_t tmp = ((uint64_t)x[k] << 29) + carry;
			if (tmp > 1000000000)
			{
				carry = tmp / 1000000000;
				x[k] = tmp % 1000000000;
			}
			else
			{
				carry = 0;
				x[k] = tmp;
			}
			if (k == (z - 1 & MASK) && k != a && !x[k]) z = k;
			if (k == a) break;
		}
		if (carry)
		{
			rp += 9;
			a = (a - 1 & MASK);
			if (a == z) {
				z = (z - 1 & MASK);
				x[z - 1 & MASK] |= x[z];
			}
			x[a] = carry;
		}
	}

	/* Downscale until exactly number of bits are left of radix point */
	for (;;)
	{
		uint32_t carry = 0;
		int sh = 1;
		for (i = 0; i < LD_B1B_DIG; i++)
		{
			k = (a + i & MASK);
			if (k == z || x[k] < th[i])
			{
				i = LD_B1B_DIG;
				break;
			}
			if (x[a + i & MASK] > th[i]) break;
		}
		if (i == LD_B1B_DIG && rp == 9 * LD_B1B_DIG) break;
		/* FIXME: find a way to compute optimal sh */
		if (rp > 9 + 9 * LD_B1B_DIG) sh = 9;
		e2 += sh;
		for (k = a; k != z; k = (k + 1 & MASK))
		{
			uint32_t tmp = x[k] & (1 << sh) - 1;
			x[k] = (x[k] >> sh) + carry;
			carry = (1000000000 >> sh) * tmp;
			if (k == a && !x[k]) {
				a = (a + 1 & MASK);
				i--;
				rp -= 9;
			}
		}
		if (carry)
		{
			if ((z + 1 & MASK) != a)
			{
				x[z] = carry;
				z = (z + 1 & MASK);
			}
			else x[z - 1 & MASK] |= 1;
		}
	}

	/* Assemble desired bits into floating point variable */
	for (y = i = 0; i < LD_B1B_DIG; i++) {
		if ((a + i & MASK) == z) x[(z = (z + 1 & MASK)) - 1] = 0;
		y = 1000000000.0L * y + x[a + i & MASK];
	}

	y *= sign;

	/* Limit precision for denormal results */
	if (bits > LDBL_MANT_DIG + e2 - emin) {
		bits = LDBL_MANT_DIG + e2 - emin;
		if (bits < 0) bits = 0;
		denormal = 1;
	}

	/* Calculate bias term to force rounding, move out lower bits */
	if (bits < LDBL_MANT_DIG) {
		bias = copysignl(scalbn(1, 2 * LDBL_MANT_DIG - bits - 1), y);
		frac = fmodl(y, scalbn(1, LDBL_MANT_DIG - bits));
		y -= frac;
		y += bias;
	}

	/* Process tail of decimal input so it can affect rounding */
	if ((a + i & MASK) != z) {
		uint32_t t = x[a + i & MASK];
		if (t < 500000000 && (t || (a + i + 1 & MASK) != z))
			frac += 0.25 * sign;
		else if (t > 500000000)
			frac += 0.75 * sign;
		else if (t == 500000000) {
			if ((a + i + 1 & MASK) == z)
				frac += 0.5 * sign;
			else
				frac += 0.75 * sign;
		}
		if (LDBL_MANT_DIG - bits >= 2 && !fmodl(frac, 1))
			frac++;
	}

	y += frac;
	y -= bias;

	if ((e2 + LDBL_MANT_DIG & INT_MAX) > emax - 5) {
		if (fabsl(y) >= 2 / LDBL_EPSILON) {
			if (denormal && bits == LDBL_MANT_DIG + e2 - emin)
				denormal = 0;
			y *= 0.5;
			e2++;
		}
		if (e2 + LDBL_MANT_DIG > emax || (denormal && frac))
			errno = ERANGE;
	}

	return scalbnl(y, e2);
}

template<class TFloatScanState>
static long double hexfloat(ErrType &err, TFloatScanState *f, int bits, int emin, int sign, bool pok)
{
	uint32_t x = 0;
	long double y = 0;
	long double scale = 1;
	long double bias = 0;
	bool gottail = false;
	bool gotrad = false;
	bool gotdig = false;
	int64_t rp = 0;
	int64_t dc = 0;
	int64_t e2 = 0;
	int d;
	std::pair<bool, int32_t> c;

	c = f->GetOne();

	/* Skip leading zeros */
	for (; isspecific(c, '0'); c = f->GetOne()) gotdig = true;

	if (isspecific(c, '.'))
	{
		gotrad = true;
		c = f->GetOne();
		/* Count zeros after the radix point before significand */
		for (rp = 0; isspecific(c, '0'); c = f->GetOne(), rp--) gotdig = true;
	}

	for (; c.first && (isdigitchar(c.second) || ishexchar(c.second) || c.second == '.'); c = f->GetOne())
	{
		if (c.second == '.')
		{
			if (gotrad) break;
			rp = dc;
			gotrad = 1;
		}
		else
		{
			gotdig = 1;
			if (c.second > '9')
			{
				d = (c.second | 32) + 10 - 'a';
			}
			else
				d = c.second - '0';
			if (dc < 8)
			{
				x = x * 16 + d;
			}
			else if (dc < LDBL_MANT_DIG / 4 + 1)
			{
				y += d * (scale /= 16);
			}
			else if (d && !gottail) {
				y += 0.5 * scale;
				gottail = 1;
			}
			dc++;
		}
	}
	if (!gotdig)
	{
		f->UnGetOne();
		if (pok) {
			f->UnGetOne();
			if (gotrad)
				f->UnGetOne();
		}
		return sign * 0.0;
	}
	if (!gotrad) rp = dc;
	while (dc < 8) x *= 16, dc++;
	if (c.first && ((c.second | 32) == 'p'))
	{
		e2 = scanexp(f, pok);
		if (e2 == std::numeric_limits<int64_t>::min())
		{
			if (pok)
			{
				f->UnGetOne();
			}
			else {
				return 0;
			}
			e2 = 0;
		}
	}
	else {
		f->UnGetOne();
	}
	e2 += 4 * rp - 32;

	if (!x) return sign * 0.0;
	if (e2 > -emin) {
		errno = ERANGE;
		return sign * LDBL_MAX * LDBL_MAX;
	}
	if (e2 < emin - 2 * LDBL_MANT_DIG) {
		errno = ERANGE;
		return sign * LDBL_MIN * LDBL_MIN;
	}

	while (x < 0x80000000) {
		if (y >= 0.5) {
			x += x + 1;
			y += y - 1;
		}
		else {
			x += x;
			y += y;
		}
		e2--;
	}

	if (bits > 32 + e2 - emin) {
		bits = 32 + e2 - emin;
		if (bits < 0) bits = 0;
	}

	if (bits < LDBL_MANT_DIG)
		bias = copysignl(scalbn(1, 32 + LDBL_MANT_DIG - bits - 1), sign);

	if (bits < 32 && y && !(x & 1)) x++, y = 0;

	y = bias + sign * (long double)x + sign * y;
	y -= bias;

	if (!y) errno = ERANGE;

	return scalbnl(y, e2);
}

template<class TFloatScanState>
static long double floatscan(ErrType &err, TFloatScanState *f, int prec, bool pok)
{
	int sign = 1;
	size_t i;
	int bits;
	int emin;
	std::pair<bool, int32_t> c;

	switch (prec)
	{
	case 0:
		bits = FLT_MANT_DIG;
		emin = FLT_MIN_EXP - bits;
		break;
	case 1:
		bits = DBL_MANT_DIG;
		emin = DBL_MIN_EXP - bits;
		break;
	case 2:
		bits = LDBL_MANT_DIG;
		emin = LDBL_MIN_EXP - bits;
		break;
	default:
		return 0;
	}

	c = f->GetOne();

	if (c.first && (c.second == '+' || c.second == '-'))
	{
		sign -= 2 * (c.second == '-');
		c = f->GetOne();
	}

	for (i = 0; i < 8 && c.first && ((c.second | 32) == "infinity"[i]); i++)
	{
		if (i < 7) c = f->GetOne();
	}
	if (i == 3 || i == 8 || (i > 3 && pok))
	{
		if (i != 8) {
			f->UnGetOne();
			if (pok) for (; i > 3; i--) f->UnGetOne();
		}
		return sign * INFINITY;
	}
	if (!i)
	{
		for (i = 0; i < 3 && c.first && ((c.second | 32) == "nan"[i]); i++)
		{
			if (i < 2)
				c = f->GetOne();
		}
	}
	if (i == 3)
	{
		if (!isspecific(f->GetOne(), '('))
		{
			f->UnGetOne();
			return NAN;
		}
		for (i = 1; ; i++)
		{
			c = f->GetOne();
			if (c.first && (isdigitchar(c.second) || ishexchar(c.second) || c.second == '_'))
				continue;
			if (isspecific(c, ')'))
				return NAN;
			f->UnGetOne();
			if (!pok)
			{
				err = ErrType::kInvalid;
				return 0;
			}
			while (i--)
				f->UnGetOne();
			return NAN;
		}
		return NAN;
	}

	if (i)
	{
		f->UnGetOne();
		err = ErrType::kInvalid;
		return 0;
	}

	if (isspecific(c, '0'))
	{
		c = f->GetOne();
		if (c.first && ((c.second | 32) == 'x'))
			return hexfloat(err, f, bits, emin, sign, pok);
		f->UnGetOne();
		c = std::pair<bool, int32_t>(true, '0');
	}

	return decfloat(err, f, c, bits, emin, sign, pok);
}

namespace rkit { namespace corelib {
	template<class TFloat, class TState>
	bool CharsToFloat(TFloat &f, TState &state, int prec)
	{
		ErrType err = ErrType::kNone;
		long double result = floatscan(err, &state, prec, true);
		if (err == ErrType::kNone)
		{
			f = static_cast<TFloat>(result);
			return true;
		}
		else
		{
			f = static_cast<TFloat>(0);
			return false;
		}
	}

	template<class TFloat, class TChar>
	bool CharsToFloatChecking(TFloat &f, const TChar *chars, size_t &inOutLen, int prec)
	{
		corelib::MemFloatScanState<TChar> state(chars, inOutLen);
		if (!CharsToFloat(f, state, prec))
		{
			inOutLen = 0;
			return false;
		}

		inOutLen = state.GetPos();
		return true;
	}
} }

bool RKIT_CORELIB_API rkit::utils::CharsToFloat(float &f, const uint8_t *chars, size_t &inOutLen)
{
	return corelib::CharsToFloatChecking(f, chars, inOutLen, 0);
}

bool RKIT_CORELIB_API rkit::utils::CharsToDouble(double &f, const uint8_t *chars, size_t &inOutLen)
{
	return corelib::CharsToFloatChecking(f, chars, inOutLen, 1);
}

bool RKIT_CORELIB_API rkit::utils::WCharsToFloat(float &f, const wchar_t *chars, size_t &inOutLen)
{
	return corelib::CharsToFloatChecking(f, chars, inOutLen, 0);
}

bool RKIT_CORELIB_API rkit::utils::WCharsToDouble(double &f, const wchar_t *chars, size_t &inOutLen)
{
	return corelib::CharsToFloatChecking(f, chars, inOutLen, 1);
}

bool RKIT_CORELIB_API rkit::utils::CharsToFloatDynamic(float &f, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state))
{
	corelib::ProxyScanState proxy(state, getOneCharCb, unGetCharCb);
	return corelib::CharsToFloat(f, proxy, 0);
}

bool RKIT_CORELIB_API rkit::utils::CharsToDoubleDynamic(double &f, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state))
{
	corelib::ProxyScanState proxy(state, getOneCharCb, unGetCharCb);
	return corelib::CharsToFloat(f, proxy, 1);
}
