#pragma once

#include <cstdint>
#include <cstddef>

#include "rkit/Core/StringProto.h"

namespace rkit::data
{
	struct ContentIDString;

	struct ContentID
	{
		static const size_t kSize = 32;

		uint8_t m_data[kSize];

		ContentIDString ToString() const;

		bool operator==(const ContentID &other) const;
		bool operator!=(const ContentID &other) const;

		bool operator<(const ContentID &other) const;
		bool operator<=(const ContentID &other) const;
		bool operator>(const ContentID &other) const;
		bool operator>=(const ContentID &other) const;
	};

	struct ContentIDString
	{
		static const size_t kLength = 50;

		char m_chars[kLength + 1];

		StringView ToStringView() const;
	};
}

#include "rkit/Core/Hasher.h"
#include "rkit/Core/StringView.h"

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



	inline ContentIDString ContentID::ToString() const
	{
		const char *chars = "0123456789abcdefghijklmnopqrstuvwxyz";

		ContentIDString result;

		uint8_t trailingBits = 0;

		size_t outIndex = 0;

		for (size_t chunkIndex = 0; chunkIndex < 8; chunkIndex++)
		{
			uint32_t chunk = m_data[chunkIndex * 4 + 0];
			chunk |= static_cast<uint32_t>(m_data[chunkIndex * 4 + 1]) << 8;
			chunk |= static_cast<uint32_t>(m_data[chunkIndex * 4 + 2]) << 16;
			chunk |= static_cast<uint32_t>(m_data[chunkIndex * 4 + 3]) << 24;

			uint8_t trailingBit = static_cast<uint8_t>((chunk >> 31) & 1u);
			trailingBits |= (trailingBit << chunkIndex);

			chunk &= 0x7ffffff;

			for (size_t outChar = 0; outChar < 6; outChar++)
			{
				uint8_t charIndex = chunk % 36;
				chunk /= 36;

				result.m_chars[chunkIndex * 6 + outChar] = chars[charIndex];
			}
		}

		result.m_chars[48] = chars[trailingBits & 0xf];
		result.m_chars[49] = chars[(trailingBits >> 4) & 0xf];
		result.m_chars[50] = '\0';

		return result;
	}

	inline StringView ContentIDString::ToStringView() const
	{
		return StringView(m_chars, kLength);
	}
}
