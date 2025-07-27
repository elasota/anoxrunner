#pragma once

#include "rkit/Core/CoreDefs.h"

#include <cstdint>

#if defined(_MSC_VER) && defined(_M_AMD64)
#include <intrin.h>
#endif

namespace rkit { namespace math {
	class UInt128
	{
	public:
		UInt128();
		explicit UInt128(uint64_t ui64);

		UInt128 operator+(const UInt128 &other) const;
		UInt128 operator-(const UInt128 &other) const;
		UInt128 operator*(const UInt128 &other) const;
		UInt128 operator<<(uint8_t bits) const;
		UInt128 operator>>(uint8_t bits) const;
		UInt128 operator|(const UInt128 &other) const;
		UInt128 operator&(const UInt128 &other) const;
		UInt128 operator^(const UInt128 &other) const;
		UInt128 operator-() const;

		UInt128& operator+=(const UInt128 &other);
		UInt128& operator-=(const UInt128 &other);
		UInt128& operator*=(const UInt128 &other);
		UInt128& operator<<=(uint8_t bits);
		UInt128& operator>>=(uint8_t bits);
		UInt128& operator|=(const UInt128 &other);
		UInt128& operator&=(const UInt128 &other);
		UInt128& operator^=(const UInt128 &other);

		bool operator<(const UInt128 &other) const;
		bool operator<=(const UInt128 &other) const;
		bool operator>(const UInt128 &other) const;
		bool operator>=(const UInt128 &other) const;
		bool operator==(const UInt128 &other) const;
		bool operator!=(const UInt128 &other) const;

		uint64_t LowPart() const;
		uint64_t HighPart() const;

		int8_t FindHighestBit() const;
		int8_t FindLowestBit() const;

		static UInt128 FromUMul64(uint64_t a, uint64_t b);
		static UInt128 FromParts(uint64_t low, uint64_t high);

	private:
		UInt128(uint64_t low, uint64_t high);

		uint64_t m_low;
		uint64_t m_high;
	};

} } // rkit::math

#include <limits>

#include "BitOps.h"

namespace rkit { namespace math {

	inline UInt128::UInt128()
		: m_low(0)
		, m_high(0)
	{
	}

	inline UInt128::UInt128(uint64_t ui64)
		: m_low(ui64)
		, m_high(0)
	{
	}

	inline UInt128::UInt128(uint64_t low, uint64_t high)
		: m_low(low)
		, m_high(high)
	{
	}

	inline UInt128 UInt128::operator+(const UInt128 &other) const
	{
		uint64_t high = m_high + other.m_high;
		uint64_t low = m_low + other.m_low;

		if (std::numeric_limits<uint64_t>::max() - m_low < other.m_low)
			high++;

		return UInt128(low, high);
	}

	inline UInt128 UInt128::operator-(const UInt128 &other) const
	{
		return (*this) + (-other);
	}

	inline UInt128 UInt128::operator*(const UInt128 &other) const
	{
		uint64_t qwords[2] = { 0, 0 };

		UInt128 result = UInt128::FromUMul64(m_low, other.m_low);
		result.m_high += m_low * other.m_high;
		result.m_high += m_high * other.m_low;

		return result;
	}

	inline UInt128 UInt128::operator<<(uint8_t bits) const
	{
		uint64_t resultHigh = 0;
		uint64_t resultLow = 0;

		bits &= 127;
		if (bits >= 64)
			resultHigh = m_low << (bits - 64);
		else if (bits == 0)
		{
			resultLow = m_low;
			resultHigh = m_high;
		}
		else
		{
			resultLow = m_low << bits;
			resultHigh = m_high << bits;
			resultHigh |= (m_low >> (64 - bits));
		}

		return UInt128(resultLow, resultHigh);
	}

	inline UInt128 UInt128::operator>>(uint8_t bits) const
	{
		uint64_t resultHigh = 0;
		uint64_t resultLow = 0;

		bits &= 127;
		if (bits >= 64)
			resultLow = m_high >> (bits - 64);
		else if (bits == 0)
		{
			resultLow = m_low;
			resultHigh = m_high;
		}
		else
		{
			resultHigh = m_high >> bits;
			resultLow = m_low >> bits;
			resultLow |= (m_high << (64 - bits));
		}

		return UInt128(resultLow, resultHigh);
	}

	inline UInt128 UInt128::operator|(const UInt128 &other) const
	{
		return UInt128(m_low | other.m_low, m_high | other.m_high);
	}

	inline UInt128 UInt128::operator&(const UInt128 &other) const
	{
		return UInt128(m_low & other.m_low, m_high & other.m_high);
	}

	inline UInt128 UInt128::operator^(const UInt128 &other) const
	{
		return UInt128(m_low ^ other.m_low, m_high ^ other.m_high);
	}

	inline UInt128 UInt128::operator-() const
	{
		return UInt128(static_cast<uint64_t>(0) - m_low, static_cast<uint64_t>(0) - m_high);
	}

	inline UInt128 &UInt128::operator+=(const UInt128 &other)
	{
		*this = (*this) + other;
		return *this;
	}

	inline UInt128 &UInt128::operator-=(const UInt128 &other)
	{
		*this = (*this) - other;
		return *this;
	}

	inline UInt128 &UInt128::operator*=(const UInt128 &other)
	{
		*this = (*this) * other;
		return *this;
	}

	inline UInt128 &UInt128::operator<<=(uint8_t bits)
	{
		*this = (*this) << bits;
		return *this;
	}

	inline UInt128 &UInt128::operator>>=(uint8_t bits)
	{
		*this = (*this) >> bits;
		return *this;
	}

	inline UInt128 &UInt128::operator|=(const UInt128 &other)
	{
		*this = (*this) | other;
		return *this;
	}

	inline UInt128 &UInt128::operator&=(const UInt128 &other)
	{
		*this = (*this) & other;
		return *this;
	}

	inline UInt128 &UInt128::operator^=(const UInt128 &other)
	{
		*this = (*this) ^ other;
		return *this;
	}

	bool UInt128::operator<(const UInt128 &other) const
	{
		if (m_high == other.m_high)
			return m_low < other.m_low;
		else
			return m_high < other.m_high;
	}

	inline bool UInt128::operator<=(const UInt128 &other) const
	{
		return !(other < (*this));
	}

	inline bool UInt128::operator>(const UInt128 &other) const
	{
		return other < (*this);
	}

	inline bool UInt128::operator>=(const UInt128 &other) const
	{
		return !((*this) < other);
	}

	inline bool UInt128::operator==(const UInt128 &other) const
	{
		return m_high == other.m_high && m_low == other.m_low;
	}

	inline bool UInt128::operator!=(const UInt128 &other) const
	{
		return !((*this) == other);
	}

	inline uint64_t UInt128::LowPart() const
	{
		return m_low;
	}

	inline uint64_t UInt128::HighPart() const
	{
		return m_high;
	}

	inline int8_t UInt128::FindHighestBit() const
	{
		if (m_high == 0)
			return bitops::FindHighestSet(m_low);
		else
			return bitops::FindHighestSet(m_high) + 64;
	}

	inline int8_t UInt128::FindLowestBit() const
	{
		if (m_low == 0)
		{
			if (m_high == 0)
				return -1;
			else
				return bitops::FindLowestSet(m_high) + 64;
		}

		return bitops::FindLowestSet(m_low);
	}

	inline UInt128 UInt128::FromUMul64(uint64_t a, uint64_t b)
	{
#if defined(_MSC_VER) && defined(_M_AMD64)
		unsigned __int64 highProduct = 0;
		uint64_t lowPart = _umul128(a, b, &highProduct);
		return UInt128(lowPart, highProduct);
#else
		const uint32_t aLow = (a & 0xffffffffu);
		const uint32_t aHigh = ((a >> 32) & 0xffffffffu);
		const uint32_t bLow = (b & 0xffffffffu);
		const uint32_t bHigh = ((b >> 32) & 0xffffffffu);

		const uint64_t lowlow = static_cast<uint64_t>(aLow) * static_cast<uint64_t>(bLow);
		const uint64_t highhigh = static_cast<uint64_t>(aHigh) * static_cast<uint64_t>(bHigh);

		UInt128 initialMixed(lowlow, highhigh);

		const uint64_t lowhigh = static_cast<uint64_t>(aLow) * static_cast<uint64_t>(bHigh);
		const uint64_t highlow = static_cast<uint64_t>(aHigh) * static_cast<uint64_t>(bLow);

		const uint64_t lowAddend0 = (lowhigh & 0xffffffffu) << 32;
		const uint64_t lowAddend1 = (highlow & 0xffffffffu) << 32;
		const uint64_t highAddend0 = (lowhigh >> 32) & 0xffffffffu;
		const uint64_t highAddend1 = (highlow >> 32) & 0xffffffffu;

		return initialMixed + UInt128(lowAddend0, highAddend0) + UInt128(lowAddend1, highAddend1);
#endif
	}

	inline UInt128 UInt128::FromParts(uint64_t low, uint64_t high)
	{
		return UInt128(low, high);
	}
} } // rkit::math
