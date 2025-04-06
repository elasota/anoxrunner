#pragma once

#include <cstdint>

namespace rkit::data
{
	struct ContentID
	{
		static const size_t kSize = 32;

		uint8_t m_data[kSize];

		bool operator==(const ContentID &other) const;
		bool operator!=(const ContentID &other) const;

		bool operator<(const ContentID &other) const;
		bool operator<=(const ContentID &other) const;
		bool operator>(const ContentID &other) const;
		bool operator>=(const ContentID &other) const;
	};
}

#include "rkit/Core/Hasher.h"

namespace rkit
{
	template<>
	struct Hasher<data::ContentID> : public BinaryHasher<data::ContentID>
	{
	};
}

namespace rkit::data
{
	inline bool ContentID::operator==(const ContentID &other) const
	{
		for (size_t i = 0; i < kSize; i++)
		{
			if (m_data[i] != other.m_data[i])
				return false;
		}

		return true;
	}

	inline bool ContentID::operator!=(const ContentID &other) const
	{
		return !((*this) == other);
	}

	inline bool ContentID::operator<(const ContentID &other) const
	{
		for (size_t i = 0; i < kSize; i++)
		{
			if (m_data[i] != other.m_data[i])
				return m_data[i] < other.m_data[i];
		}

		return false;
	}

	inline bool ContentID::operator<=(const ContentID &other) const
	{
		return !(other < (*this));
	}

	inline bool ContentID::operator>(const ContentID &other) const
	{
		return other < (*this);
	}

	inline bool ContentID::operator>=(const ContentID &other) const
	{
		return !((*this) < other);
	}
}
