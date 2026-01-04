#pragma once

#include <cstddef>
#include <cstdint>

namespace rkit
{
	class StaticBoolArrayIterator
	{
	public:
		StaticBoolArrayIterator();
		StaticBoolArrayIterator(const uint8_t *pByte, uint8_t subBit);

		StaticBoolArrayIterator operator--(int);
		StaticBoolArrayIterator &operator--();

		StaticBoolArrayIterator operator++(int);
		StaticBoolArrayIterator &operator++();

		StaticBoolArrayIterator &operator+=(ptrdiff_t diff);
		StaticBoolArrayIterator &operator+=(size_t diff);

		StaticBoolArrayIterator &operator-=(ptrdiff_t diff);
		StaticBoolArrayIterator &operator-=(size_t diff);

		StaticBoolArrayIterator operator+(ptrdiff_t diff) const;
		StaticBoolArrayIterator operator+(size_t diff) const;

		StaticBoolArrayIterator operator-(ptrdiff_t diff) const;
		StaticBoolArrayIterator operator-(size_t diff) const;

		bool operator==(const StaticBoolArrayIterator &other) const;
		bool operator!=(const StaticBoolArrayIterator &other) const;

		StaticBoolArrayIterator &operator=(const StaticBoolArrayIterator &other);

		bool operator*() const;

	private:
		const uint8_t *m_pByte;
		uint8_t m_subBit;
	};

	template<size_t TSize>
	class StaticBoolArray
	{
	public:
		static const size_t kSize = TSize;

		StaticBoolArray();
		StaticBoolArray(const StaticBoolArray<TSize> &other) = default;

		bool Get(size_t index) const;
		void Set(size_t index, bool value);

		StaticBoolArrayIterator begin() const;
		StaticBoolArrayIterator end() const;

		StaticBoolArray<TSize> operator|(const StaticBoolArray<TSize> &other) const;
		StaticBoolArray<TSize> &operator|=(const StaticBoolArray<TSize> &other);

		StaticBoolArray<TSize> operator&(const StaticBoolArray<TSize> &other) const;
		StaticBoolArray<TSize> &operator&=(const StaticBoolArray<TSize> &other);

		StaticBoolArray<TSize> operator^(const StaticBoolArray<TSize> &other) const;
		StaticBoolArray<TSize> &operator^=(const StaticBoolArray<TSize> &other);

		StaticBoolArray<TSize> operator~() const;

		bool operator==(const StaticBoolArray<TSize> &other) const;
		bool operator!=(const StaticBoolArray<TSize> &other) const;

		bool FindFirstSet(size_t &outIndex) const;
		bool FindLastSet(size_t &outIndex) const;

		static StaticBoolArray<TSize> FromBitsUnchecked(const uint8_t *bits);

	private:
		explicit StaticBoolArray(const uint8_t *bits);

		static const size_t kNumBytes = (TSize + 7) / 8;

		uint8_t m_bytes[kNumBytes];
	};
}

#include "RKitAssert.h"

#include <limits>
#include <type_traits>

namespace rkit
{
	inline StaticBoolArrayIterator::StaticBoolArrayIterator()
		: m_pByte(nullptr)
		, m_subBit(0)
	{
	}

	inline StaticBoolArrayIterator::StaticBoolArrayIterator(const uint8_t *pByte, uint8_t subBit)
		: m_pByte(pByte)
		, m_subBit(subBit)
	{
	}

	inline StaticBoolArrayIterator StaticBoolArrayIterator::operator--(int)
	{
		StaticBoolArrayIterator clone = *this;
		(*this) -= static_cast<size_t>(1);
		return clone;
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator--()
	{
		return (*this) -= static_cast<size_t>(1);
	}

	inline StaticBoolArrayIterator StaticBoolArrayIterator::operator++(int)
	{
		StaticBoolArrayIterator clone = *this;
		(*this) += static_cast<size_t>(1);
		return clone;
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator++()
	{
		return (*this) += static_cast<size_t>(1);
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator+=(ptrdiff_t diff)
	{
		(*this) = (*this) + diff;
		return *this;
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator+=(size_t diff)
	{
		(*this) = (*this) + diff;
		return *this;
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator-=(ptrdiff_t diff)
	{
		(*this) = (*this) - diff;
		return *this;
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator-=(size_t diff)
	{
		(*this) = (*this) - diff;
		return *this;
	}

	inline StaticBoolArrayIterator StaticBoolArrayIterator::operator+(ptrdiff_t diff) const
	{
		if (diff < 0)
			return (*this) - static_cast<size_t>(-diff);
		else
			return (*this) + static_cast<size_t>(diff);
	}

	inline StaticBoolArrayIterator StaticBoolArrayIterator::operator+(size_t diff) const
	{
		RKIT_ASSERT(m_pByte != nullptr);
		uint8_t newSubBit = static_cast<uint8_t>(m_subBit + (diff & 7u));

		return StaticBoolArrayIterator(m_pByte + (diff / 8u) + (newSubBit / 8u), static_cast<uint8_t>(newSubBit & 7));
	}

	inline StaticBoolArrayIterator StaticBoolArrayIterator::operator-(ptrdiff_t diff) const
	{
		if (diff < 0)
			return (*this) + static_cast<size_t>(-diff);
		else
			return (*this) - static_cast<size_t>(diff);
	}

	inline StaticBoolArrayIterator StaticBoolArrayIterator::operator-(size_t diff) const
	{
		RKIT_ASSERT(m_pByte != nullptr);

		const uint8_t subBitWithBorrow = m_subBit + 8 - (diff & 7u);

		return StaticBoolArrayIterator(m_pByte + ((subBitWithBorrow / 8u) - 1) - (diff / 8u), (subBitWithBorrow & 7u));
	}

	inline bool StaticBoolArrayIterator::operator==(const StaticBoolArrayIterator &other) const
	{
		return m_pByte == other.m_pByte && m_subBit == other.m_subBit;
	}

	inline bool StaticBoolArrayIterator::operator!=(const StaticBoolArrayIterator &other) const
	{
		return !((*this) == other);
	}

	inline StaticBoolArrayIterator &StaticBoolArrayIterator::operator=(const StaticBoolArrayIterator &other)
	{
		m_pByte = other.m_pByte;
		m_subBit = other.m_subBit;
		return *this;
	}

	inline bool StaticBoolArrayIterator::operator*() const
	{
		RKIT_ASSERT(m_pByte != nullptr);
		return (((*m_pByte) >> m_subBit) & 1) != 0;
	}

	template<size_t TSize>
	StaticBoolArray<TSize>::StaticBoolArray()
		: m_bytes{}
	{
		static_assert(std::is_standard_layout<StaticBoolArray<TSize>>::value, "StaticBoolArray should be standard layout");
	}

	template<size_t TSize>
	void StaticBoolArray<TSize>::Set(size_t index, bool value)
	{
		RKIT_ASSERT(index < TSize);

		if (value)
			m_bytes[index / 8u] |= static_cast<uint8_t>(1 << (index % 8u));
		else
			m_bytes[index / 8u] &= ~static_cast<uint8_t>(1 << (index % 8u));
	}

	template<size_t TSize>
	bool StaticBoolArray<TSize>::Get(size_t index) const
	{
		RKIT_ASSERT(index < TSize);

		return (m_bytes[index / 8u] & (1 << (index % 8u))) != 0;
	}


	template<size_t TSize>
	StaticBoolArrayIterator StaticBoolArray<TSize>::begin() const
	{
		return StaticBoolArrayIterator(m_bytes, 0);
	}

	template<size_t TSize>
	StaticBoolArrayIterator StaticBoolArray<TSize>::end() const
	{
		return StaticBoolArrayIterator(m_bytes + TSize / 8u, TSize % 8u);
	}

	template<size_t TSize>
	StaticBoolArray<TSize> StaticBoolArray<TSize>::operator|(const StaticBoolArray<TSize> &other) const
	{
		StaticBoolArray<TSize> result = (*this);
		result |= other;
		return result;
	}

	template<size_t TSize>
	StaticBoolArray<TSize> &StaticBoolArray<TSize>::operator|=(const StaticBoolArray<TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] |= other.m_bytes[i];

		return *this;
	}

	template<size_t TSize>
	StaticBoolArray<TSize> StaticBoolArray<TSize>::operator&(const StaticBoolArray<TSize> &other) const
	{
		StaticBoolArray<TSize> result = (*this);
		result &= other;
		return result;
	}


	template<size_t TSize>
	StaticBoolArray<TSize> &StaticBoolArray<TSize>::operator&=(const StaticBoolArray<TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] &= other.m_bytes[i];

		return *this;
	}

	template<size_t TSize>
	StaticBoolArray<TSize> StaticBoolArray<TSize>::operator^(const StaticBoolArray<TSize> &other) const
	{
		StaticBoolArray<TSize> result = (*this);
		result ^= other;
		return result;
	}

	template<size_t TSize>
	StaticBoolArray<TSize> &StaticBoolArray<TSize>::operator^=(const StaticBoolArray<TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] ^= other.m_bytes[i];

		const size_t trailingBitsCount = TSize % 8u;
		if (trailingBitsCount != 0)
			m_bytes[TSize / 8u] &= static_cast<uint8_t>((1 << trailingBitsCount) - 1);

		return *this;
	}

	template<size_t TSize>
	StaticBoolArray<TSize> StaticBoolArray<TSize>::operator~() const
	{
		StaticBoolArray<TSize> result;

		for (size_t i = 0; i < TSize; i++)
			result.m_bytes[i] = ~m_bytes[i];

		const size_t trailingBitsCount = TSize % 8u;
		if (trailingBitsCount != 0)
			result.m_bytes[TSize / 8u] &= static_cast<uint8_t>((1 << trailingBitsCount) - 1);

		return *this;
	}

	template<size_t TSize>
	bool StaticBoolArray<TSize>::operator==(const StaticBoolArray<TSize> &other) const
	{
		for (size_t i = 0; i < TSize; i++)
		{
			if (m_bytes[i] != other.m_bytes[i])
				return false;
		}

		return true;
	}

	template<size_t TSize>
	bool StaticBoolArray<TSize>::operator!=(const StaticBoolArray<TSize> &other) const
	{
		return !((*this) == other);
	}

	template<size_t TSize>
	bool StaticBoolArray<TSize>::FindLastSet(size_t &outIndex) const
	{
		size_t index = TSize;
		while (index > 0)
		{
			--index;
			if (Get(index))
			{
				outIndex = index;
				return true;
			}
		}

		return false;
	}

	template<size_t TSize>
	StaticBoolArray<TSize> StaticBoolArray<TSize>::FromBitsUnchecked(const uint8_t *bits)
	{
		return StaticBoolArray<TSize>(bits);
	}

	template<size_t TSize>
	StaticBoolArray<TSize>::StaticBoolArray(const uint8_t *bits)
		: m_bytes{}
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] = bits[i];
	}

	template<size_t TSize>
	bool StaticBoolArray<TSize>::FindFirstSet(size_t &outIndex) const
	{
		size_t index = 0;
		while (index < TSize)
		{
			if (Get(index))
			{
				outIndex = index;
				return true;
			}
			++index;
		}

		return false;
	}
}
