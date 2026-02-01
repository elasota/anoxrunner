#pragma once

#include <type_traits>

#include "CharacterEncoding.h"

namespace rkit { namespace priv {

	template<class TChar, class TUCanonical, bool TCharIsUnsigned, bool TCharIsLarger, bool TCharIsSameSize>
	struct UnicodeCharacterHelper2
	{
	};

	// Unsigned char larger than canonical
	template<class TChar, class TUCanonical>
	struct UnicodeCharacterHelper2<TChar, TUCanonical, true, true, false>
	{
		static bool CheckMakeCanonical(TChar ch, TUCanonical &outCh);
	};

	// Signed char larger than canonical
	template<class TChar, class TUCanonical>
	struct UnicodeCharacterHelper2<TChar, TUCanonical, false, true, false>
	{
		static bool CheckMakeCanonical(TChar ch, TUCanonical &outCh);
	};

	template<class TChar, class TUCanonical, bool TCharIsUnsigned>
	struct UnicodeCharacterHelper2<TChar, TUCanonical, TCharIsUnsigned, false, true>
	{
		static bool CheckMakeCanonical(TChar ch, TUCanonical &outCh);
	};

	template<class TChar, class TUCanonical>
	struct UnicodeCharacterHelper : public UnicodeCharacterHelper2<TChar, TUCanonical, std::is_unsigned<TChar>::value, (sizeof(TChar) > sizeof(TUCanonical)), (sizeof(TChar) == sizeof(TUCanonical))>
	{
	};
} }

namespace rkit { namespace priv {
	template<CharacterEncoding TEncoding>
	struct CharacterEncodingValidatorImpl
	{
	};

	template<>
	struct CharacterEncodingValidatorImpl<CharacterEncoding::kASCII>
	{
		template<class TChar>
		static bool ValidateSpan(const Span<const TChar> &span);
	};

	template<>
	struct CharacterEncodingValidatorImpl<CharacterEncoding::kUTF8>
	{
		template<class TChar>
		static bool ValidateSpan(const Span<const TChar> &span);
	};

	template<>
	struct CharacterEncodingValidatorImpl<CharacterEncoding::kUTF16>
	{
		template<class TChar>
		static bool ValidateSpan(const Span<const TChar> &span);
	};

	template<>
	struct CharacterEncodingValidatorImpl<CharacterEncoding::kByte>
	{
		template<class TChar>
		static bool ValidateSpan(const Span<const TChar> &span);
	};
} }

namespace rkit
{
	template<class T>
	class Span;

	template<CharacterEncoding TEncoding>
	struct CharacterEncodingValidator
	{
		template<class TChar>
		static bool ValidateSpan(const Span<TChar> &span);
	};
}

#include <stdint.h>

#include "TypeTraits.h"

namespace rkit { namespace priv {

	template<class TChar, class TUCanonical>
	bool UnicodeCharacterHelper2<TChar, TUCanonical, true, true, false>::CheckMakeCanonical(TChar ch, TUCanonical &outCh)
	{
		// Unsigned, char is larger
		if (ch > std::numeric_limits<TUCanonical>::max())
			return false;

		outCh = static_cast<TUCanonical>(ch);
		return true;
	}

	template<class TChar, class TUCanonical>
	bool UnicodeCharacterHelper2<TChar, TUCanonical, false, true, false>::CheckMakeCanonical(TChar ch, TUCanonical &outCh)
	{
		// Signed, char is larger
		if (ch < 0 || ch > static_cast<TChar>(std::numeric_limits<TUCanonical>::max()))
			return false;

		outCh = static_cast<TUCanonical>(ch);
		return true;
	}

	template<class TChar, class TUCanonical, bool TCharIsUnsigned>
	bool UnicodeCharacterHelper2<TChar, TUCanonical, TCharIsUnsigned, false, true>::CheckMakeCanonical(TChar ch, TUCanonical &outCh)
	{
		// Same size
		outCh = static_cast<TUCanonical>(ch);
		return true;
	}
} }

namespace rkit { namespace priv {
	template<class TChar>
	bool CharacterEncodingValidatorImpl<CharacterEncoding::kASCII>::ValidateSpan(const Span<const TChar> &span)
	{
		for (const TChar &ch : span)
		{
			uint8_t asciiChar = 0;
			if (!priv::UnicodeCharacterHelper<TChar, uint8_t>::CheckMakeCanonical(ch, asciiChar))
				return false;

			if (asciiChar >= 128)
				return false;
		}

		return true;
	}

	template<class TChar>
	bool CharacterEncodingValidatorImpl<CharacterEncoding::kUTF8>::ValidateSpan(const Span<const TChar> &span)
	{
		size_t remaining = span.Count();
		const TChar *chars = span.Ptr();

		while (remaining > 0)
		{
			uint8_t width = 1;
			uint8_t firstByte = 0;

			if (!priv::UnicodeCharacterHelper<TChar, uint8_t>::CheckMakeCanonical(*chars, firstByte))
				return false;

			if (firstByte & 0x80)
			{
				while (width < 5)
				{
					width++;
					const uint8_t upperBitMask = (0xf8 << width) & 0xff;
					const uint8_t upperBitRequirement = (0xf0 << width) & 0xff;

					if ((firstByte & upperBitMask) == upperBitRequirement)
					{
						if ((firstByte ^ upperBitRequirement) == 0)
							return false;	// Suboptimal

						break;
					}
				}

				if (width == 5)
					return false;	// Invalid

				if (remaining < width)
					return false;	// Not enough bytes

				for (uint8_t i = 1; i < width; i++)
				{
					uint8_t nextByte = 0;
					if (!priv::UnicodeCharacterHelper<TChar, uint8_t>::CheckMakeCanonical(chars[i], nextByte))
						return false;

					if ((nextByte & 0xc0) != 0x80)
						return false;	// Invalid
				}
			}

			remaining -= width;
			chars += width;
		}

		return true;
	}

	template<class TChar>
	bool CharacterEncodingValidatorImpl<CharacterEncoding::kUTF16>::ValidateSpan(const Span<const TChar> &span)
	{
		size_t remaining = span.Count();
		const TChar *chars = span.Ptr();

		while (remaining > 0)
		{
			uint16_t width = 1;
			uint16_t firstWord = 0;

			if (!priv::UnicodeCharacterHelper<TChar, uint16_t>::CheckMakeCanonical(*chars, firstWord))
				return false;

			if ((firstWord & 0xfc00) == 0xd800)
			{
				if (remaining < 2)
					return false;	// Truncated

				uint16_t secondWord = 0;

				if (!priv::UnicodeCharacterHelper<TChar, uint16_t>::CheckMakeCanonical(chars[1], secondWord))
					return false;

				if ((secondWord & 0xfc00) != 0xdc00)
					return false;

				width = 2;
			}
			else if ((firstWord & 0xfc00) == 0xdc00)
				return false;

			remaining -= width;
			chars += width;
		}

		return true;
	}

	template<class TChar>
	bool CharacterEncodingValidatorImpl<CharacterEncoding::kByte>::ValidateSpan(const Span<const TChar> &span)
	{
		return true;
	}
} }


namespace rkit
{
	template<CharacterEncoding TEncoding>
	template<class TChar>
	bool CharacterEncodingValidator<TEncoding>::ValidateSpan(const Span<TChar> &span)
	{
		return priv::CharacterEncodingValidatorImpl<TEncoding>::template ValidateSpan<typename RemoveConst<TChar>::Type_t>(span);
	}
}
