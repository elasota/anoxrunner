#pragma once

#include <cstddef>
#include <cstdint>

namespace rkit::render::vulkan
{
	class QueueMask
	{
	public:
		QueueMask();

		bool Get(size_t index) const;
		void Set(size_t index, bool value);

		void Clear();

		bool FindFirstSet(size_t &outIndex) const;

		QueueMask operator&(const QueueMask &other) const;
		QueueMask operator|(const QueueMask &other) const;
		QueueMask operator^(const QueueMask &other) const;

		QueueMask &operator&=(const QueueMask &other);
		QueueMask &operator|=(const QueueMask &other);
		QueueMask &operator^=(const QueueMask &other);

		bool operator==(const QueueMask &other) const;
		bool operator!=(const QueueMask &other) const;

	private:
		typedef uint64_t Bits_t;

		static const size_t kNumBits = sizeof(Bits_t) * 8;

		explicit QueueMask(Bits_t bits);

		Bits_t m_bits;
	};
}

#include "rkit/Core/RKitAssert.h"

namespace rkit::render::vulkan
{
	inline QueueMask::QueueMask()
		: m_bits(0)
	{
	}

	inline QueueMask::QueueMask(Bits_t bits)
		: m_bits(bits)
	{
	}

	inline bool QueueMask::Get(size_t index) const
	{
		if (index >= kNumBits)
			return false;

		return ((m_bits >> index) & 1) != 0;
	}

	inline void QueueMask::Set(size_t index, bool value)
	{
		Bits_t mask = static_cast<Bits_t>(static_cast<Bits_t>(1) << index);

		if (value)
			m_bits |= mask;
		else
			m_bits &= ~mask;
	}

	inline void QueueMask::Clear()
	{
		m_bits = 0;
	}

	inline bool QueueMask::FindFirstSet(size_t &outIndex) const
	{
		if (m_bits == 0)
			return false;

		size_t index = 0;
		while (!Get(index))
			index++;

		outIndex = index;

		return true;
	}

	inline QueueMask QueueMask::operator&(const QueueMask &other) const
	{
		return QueueMask(m_bits & other.m_bits);
	}

	inline QueueMask QueueMask::operator|(const QueueMask &other) const
	{
		return QueueMask(m_bits | other.m_bits);
	}

	inline QueueMask QueueMask::operator^(const QueueMask &other) const
	{
		return QueueMask(m_bits ^ other.m_bits);
	}

	inline QueueMask &QueueMask::operator&=(const QueueMask &other)
	{
		m_bits &= other.m_bits;
		return *this;
	}

	inline QueueMask &QueueMask::operator|=(const QueueMask &other)
	{
		m_bits |= other.m_bits;
		return *this;
	}

	inline QueueMask &QueueMask::operator^=(const QueueMask &other)
	{
		m_bits ^= other.m_bits;
		return *this;
	}

	inline bool QueueMask::operator==(const QueueMask &other) const
	{
		return m_bits == other.m_bits;
	}

	inline bool QueueMask::operator!=(const QueueMask &other) const
	{
		return !((*this) == other);
	}
}
