#pragma once

#include <cstddef>
#include <cstdint>

namespace rkit
{
	class StaticBoolVectorIterator
	{
	public:
		StaticBoolVectorIterator();
		StaticBoolVectorIterator(const uint8_t *pByte, uint8_t subBit);

		StaticBoolVectorIterator operator--(int);
		StaticBoolVectorIterator &operator--();

		StaticBoolVectorIterator operator++(int);
		StaticBoolVectorIterator &operator++();

		StaticBoolVectorIterator &operator+=(ptrdiff_t diff);
		StaticBoolVectorIterator &operator+=(size_t diff);

		StaticBoolVectorIterator &operator-=(ptrdiff_t diff);
		StaticBoolVectorIterator &operator-=(size_t diff);

		StaticBoolVectorIterator operator+(ptrdiff_t diff) const;
		StaticBoolVectorIterator operator+(size_t diff) const;

		StaticBoolVectorIterator operator-(ptrdiff_t diff) const;
		StaticBoolVectorIterator operator-(size_t diff) const;

		bool operator==(const StaticBoolVectorIterator &other) const;
		bool operator!=(const StaticBoolVectorIterator &other) const;

		StaticBoolVectorIterator &operator=(const StaticBoolVectorIterator &other);

		bool operator*() const;

	private:
		const uint8_t *m_pByte;
		uint8_t m_subBit;
	};

	template<size_t TSize>
	class StaticBoolVector
	{
	public:
		static const size_t kSize = TSize;

		StaticBoolVector();

		bool Get(size_t index) const;
		void Set(size_t index, bool value);

		StaticBoolVectorIterator begin() const;
		StaticBoolVectorIterator end() const;

		StaticBoolVector<TSize> operator|(const StaticBoolVector<TSize> &other) const;
		StaticBoolVector<TSize> &operator|=(const StaticBoolVector<TSize> &other);

		StaticBoolVector<TSize> operator&(const StaticBoolVector<TSize> &other) const;
		StaticBoolVector<TSize> &operator&=(const StaticBoolVector<TSize> &other);

		StaticBoolVector<TSize> operator^(const StaticBoolVector<TSize> &other) const;
		StaticBoolVector<TSize> &operator^=(const StaticBoolVector<TSize> &other);

		StaticBoolVector<TSize> operator~() const;

		bool operator==(const StaticBoolVector<TSize> &other) const;
		bool operator!=(const StaticBoolVector<TSize> &other) const;

		bool FindFirstSet(size_t &outIndex) const;
		bool FindLastSet(size_t &outIndex) const;

	private:
		static const size_t kNumBytes = (TSize + 7) / 8;

		uint8_t m_bytes[kNumBytes];
	};
}

#include "RKitAssert.h"

#include <limits>

namespace rkit
{
	inline StaticBoolVectorIterator::StaticBoolVectorIterator()
		: m_pByte(nullptr)
		, m_subBit(0)
	{
	}

	inline StaticBoolVectorIterator::StaticBoolVectorIterator(const uint8_t *pByte, uint8_t subBit)
		: m_pByte(pByte)
		, m_subBit(subBit)
	{
	}

	inline StaticBoolVectorIterator StaticBoolVectorIterator::operator--(int)
	{
		StaticBoolVectorIterator clone = *this;
		(*this) -= static_cast<size_t>(1);
		return clone;
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator--()
	{
		return (*this) -= static_cast<size_t>(1);
	}

	inline StaticBoolVectorIterator StaticBoolVectorIterator::operator++(int)
	{
		StaticBoolVectorIterator clone = *this;
		(*this) += static_cast<size_t>(1);
		return clone;
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator++()
	{
		return (*this) += static_cast<size_t>(1);
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator+=(ptrdiff_t diff)
	{
		(*this) = (*this) + diff;
		return *this;
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator+=(size_t diff)
	{
		(*this) = (*this) + diff;
		return *this;
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator-=(ptrdiff_t diff)
	{
		(*this) = (*this) - diff;
		return *this;
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator-=(size_t diff)
	{
		(*this) = (*this) - diff;
		return *this;
	}

	inline StaticBoolVectorIterator StaticBoolVectorIterator::operator+(ptrdiff_t diff) const
	{
		if (diff < 0)
			return (*this) - static_cast<size_t>(-diff);
		else
			return (*this) + static_cast<size_t>(diff);
	}

	inline StaticBoolVectorIterator StaticBoolVectorIterator::operator+(size_t diff) const
	{
		RKIT_ASSERT(m_pByte != nullptr);
		uint8_t newSubBit = static_cast<uint8_t>(m_subBit + (diff & 7u));

		return StaticBoolVectorIterator(m_pByte + (diff / 8u) + (newSubBit / 8u), static_cast<uint8_t>(newSubBit & 7));
	}

	inline StaticBoolVectorIterator StaticBoolVectorIterator::operator-(ptrdiff_t diff) const
	{
		if (diff < 0)
			return (*this) + static_cast<size_t>(-diff);
		else
			return (*this) - static_cast<size_t>(diff);
	}

	inline StaticBoolVectorIterator StaticBoolVectorIterator::operator-(size_t diff) const
	{
		RKIT_ASSERT(m_pByte != nullptr);

		const uint8_t subBitWithBorrow = m_subBit + 8 - (diff & 7u);

		return StaticBoolVectorIterator(m_pByte + ((subBitWithBorrow / 8u) - 1) - (diff / 8u), (subBitWithBorrow & 7u));
	}

	inline bool StaticBoolVectorIterator::operator==(const StaticBoolVectorIterator &other) const
	{
		return m_pByte == other.m_pByte && m_subBit == other.m_subBit;
	}

	inline bool StaticBoolVectorIterator::operator!=(const StaticBoolVectorIterator &other) const
	{
		return !((*this) == other);
	}

	inline StaticBoolVectorIterator &StaticBoolVectorIterator::operator=(const StaticBoolVectorIterator &other)
	{
		m_pByte = other.m_pByte;
		m_subBit = other.m_subBit;
		return *this;
	}

	inline bool StaticBoolVectorIterator::operator*() const
	{
		RKIT_ASSERT(m_pByte != nullptr);
		return (((*m_pByte) >> m_subBit) & 1) != 0;
	}

	template<size_t TSize>
	StaticBoolVector<TSize>::StaticBoolVector()
		: m_bytes{}
	{
	}

	template<size_t TSize>
	void StaticBoolVector<TSize>::Set(size_t index, bool value)
	{
		RKIT_ASSERT(index < TSize);

		if (value)
			m_bytes[index / 8u] |= static_cast<uint8_t>(1 << (index % 8u));
		else
			m_bytes[index / 8u] &= ~static_cast<uint8_t>(1 << (index % 8u));
	}

	template<size_t TSize>
	bool StaticBoolVector<TSize>::Get(size_t index) const
	{
		RKIT_ASSERT(index < TSize);

		return (m_bytes[index / 8u] & (1 << (index % 8u))) != 0;
	}


	template<size_t TSize>
	StaticBoolVectorIterator StaticBoolVector<TSize>::begin() const
	{
		return StaticBoolVectorIterator(m_bytes, 0);
	}

	template<size_t TSize>
	StaticBoolVectorIterator StaticBoolVector<TSize>::end() const
	{
		return StaticBoolVectorIterator(m_bytes + TSize / 8u, TSize % 8u);
	}

	template<size_t TSize>
	StaticBoolVector<TSize> StaticBoolVector<TSize>::operator|(const StaticBoolVector<TSize> &other) const
	{
		StaticBoolVector<TSize> result = (*this);
		result |= other;
		return result;
	}

	template<size_t TSize>
	StaticBoolVector<TSize> &StaticBoolVector<TSize>::operator|=(const StaticBoolVector<TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] |= other.m_bytes[i];

		return *this;
	}

	template<size_t TSize>
	StaticBoolVector<TSize> StaticBoolVector<TSize>::operator&(const StaticBoolVector<TSize> &other) const
	{
		StaticBoolVector<TSize> result = (*this);
		result &= other;
		return result;
	}


	template<size_t TSize>
	StaticBoolVector<TSize> &StaticBoolVector<TSize>::operator&=(const StaticBoolVector<TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] &= other.m_bytes[i];

		return *this;
	}

	template<size_t TSize>
	StaticBoolVector<TSize> StaticBoolVector<TSize>::operator^(const StaticBoolVector<TSize> &other) const
	{
		StaticBoolVector<TSize> result = (*this);
		result ^= other;
		return result;
	}

	template<size_t TSize>
	StaticBoolVector<TSize> &StaticBoolVector<TSize>::operator^=(const StaticBoolVector<TSize> &other)
	{
		for (size_t i = 0; i < TSize; i++)
			m_bytes[i] ^= other.m_bytes[i];

		const size_t trailingBitsCount = TSize % 8u;
		if (trailingBitsCount != 0)
			m_bytes[TSize / 8u] &= static_cast<uint8_t>((1 << trailingBitsCount) - 1);

		return *this;
	}

	template<size_t TSize>
	StaticBoolVector<TSize> StaticBoolVector<TSize>::operator~() const
	{
		StaticBoolVector<TSize> result;

		for (size_t i = 0; i < TSize; i++)
			result.m_bytes[i] = ~m_bytes[i];

		const size_t trailingBitsCount = TSize % 8u;
		if (trailingBitsCount != 0)
			result.m_bytes[TSize / 8u] &= static_cast<uint8_t>((1 << trailingBitsCount) - 1);

		return *this;
	}

	template<size_t TSize>
	bool StaticBoolVector<TSize>::operator==(const StaticBoolVector<TSize> &other) const
	{
		for (size_t i = 0; i < TSize; i++)
		{
			if (m_bytes[i] != other.m_bytes[i])
				return false;
		}

		return true;
	}

	template<size_t TSize>
	bool StaticBoolVector<TSize>::operator!=(const StaticBoolVector<TSize> &other) const
	{
		return !((*this) == other);
	}

	template<size_t TSize>
	bool StaticBoolVector<TSize>::FindLastSet(size_t &outIndex) const
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
	bool StaticBoolVector<TSize>::FindFirstSet(size_t &outIndex) const
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
