#pragma once

#include <cstdint>
#include <cstddef>

namespace rkit
{
	namespace UTF8
	{
		static const unsigned int kMaxEncodedBytes = 4;

		inline bool Decode(const uint8_t *characters, size_t availableCharacters, size_t &outCharactersDigested, uint32_t &outCodePoint)
		{
			if (availableCharacters <= 0)
				return false;

			if ((characters[0] & 0x80) == 0x00)
			{
				outCharactersDigested = 1;
				outCodePoint = characters[0];
				return true;
			}

			size_t sz = 0;
			uint32_t codePoint = 0;
			uint32_t minCodePoint = 0;
			if ((characters[0] & 0xe0) == 0xc0)
			{
				sz = 2;
				minCodePoint = 0x80;
				codePoint = (characters[0] & 0x1f);
			}
			else if ((characters[0] & 0xf0) == 0xe0)
			{
				sz = 3;
				minCodePoint = 0x800;
				codePoint = (characters[0] & 0x0f);
			}
			else if ((characters[0] & 0xf8) == 0xf0)
			{
				sz = 4;
				minCodePoint = 0x10000;
				codePoint = (characters[0] & 0x07);
			}
			else
				return false;

			if (availableCharacters < sz)
				return false;

			for (size_t auxByte = 1; auxByte < sz; auxByte++)
			{
				if ((characters[auxByte] & 0xc0) != 0x80)
					return false;

				codePoint = (codePoint << 6) | (characters[auxByte] & 0x3f);
			}

			if (codePoint < minCodePoint || codePoint > 0x10ffff)
				return false;

			if (codePoint >= 0xd800 && codePoint <= 0xdfff)
				return false;

			outCodePoint = codePoint;
			outCharactersDigested = sz;

			return true;
		}

		inline void Encode(uint8_t *characters, size_t &outCharactersEmitted, uint32_t codePoint)
		{
			codePoint &= 0x1fffff;

			uint8_t signalBits = 0;
			size_t numBytes = 0;
			if (codePoint < 0x0080)
			{
				numBytes = 1;
				signalBits = 0;
			}
			else if (codePoint < 0x0800)
			{
				numBytes = 2;
				signalBits = 0xc0;
			}
			else if (codePoint < 0x10000)
			{
				numBytes = 3;
				signalBits = 0xe0;
			}
			else
			{
				numBytes = 4;
				signalBits = 0xf0;
			}

			characters[0] = static_cast<uint8_t>((codePoint >> (6 * (numBytes - 1))) | signalBits);

			for (size_t i = 1; i < numBytes; i++)
			{
				const uint32_t isolate = ((codePoint >> (6 * (numBytes - 1 - i))) & 0x3f) | 0x80;
				characters[i] = static_cast<uint8_t>(isolate);
			}

			outCharactersEmitted = numBytes;
		}
	}

	namespace UTF16
	{
		inline bool Decode(const uint16_t *characters, size_t availableCharacters, size_t &outCharactersDigested, uint32_t &outCodePoint)
		{
			if (availableCharacters <= 0)
				return false;

			if ((characters[0] & 0xff80) == 0x00)
			{
				outCharactersDigested = 1;
				outCodePoint = characters[0];
				return true;
			}

			if (characters[0] <= 0xd7ff || characters[0] >= 0xe000)
			{
				outCharactersDigested = 1;
				outCodePoint = characters[0];
				return true;
			}

			// Surrogate pair
			if (characters[0] >= 0xdc00 || availableCharacters < 2)
				return false;

			if (characters[1] < 0xdc00 || characters[1] >= 0xe000)
				return false;

			uint16_t highBits = (characters[0] & 0x3ff);
			uint16_t lowBits = (characters[1] & 0x3ff);

			outCharactersDigested = 2;
			outCodePoint = (highBits << 10) + lowBits + 0x10000;

			return true;
		}

		inline void Encode(uint16_t *characters, size_t &outCharactersEmitted, uint32_t codePoint)
		{
			if (codePoint <= 0xd7ff || codePoint >= 0xe000)
			{
				outCharactersEmitted = 1;
				characters[0] = static_cast<uint16_t>(codePoint);
				return;
			}

			uint32_t codePointBits = (codePoint - 0x10000) & 0xfffff;
			uint16_t lowBits = (codePointBits & 0x3ff);
			uint16_t highBits = ((codePointBits >> 10) & 0x3ff);

			outCharactersEmitted = 2;
			characters[0] = (0xd800 + highBits);
			characters[1] = (0xdc00 + lowBits);
		}
	}
}
