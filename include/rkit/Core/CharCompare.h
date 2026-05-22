#pragma once

#include "CoreDefs.h"
#include "CharacterEncoding.h"
#include "Span.h"

#include <compare>

namespace rkit
{
	namespace priv
	{
		template<class TChar, class TCanonicalChar, size_t TSizeOfTChar = sizeof(TChar)>
		struct CharCompareAs
		{
			static std::strong_ordering CompareOrdered(const TChar *str1, const TChar *str2, size_t len);
			static bool CompareEqual(const TChar *str1, const TChar *str2, size_t len);
		};

		template<class TChar>
		struct CharCompareAs<TChar, uint8_t, 1>
		{
			static std::strong_ordering CompareOrdered(const TChar *str1, const TChar *str2, size_t len);
			static bool CompareEqual(const TChar *str1, const TChar *str2, size_t len);
		};

		template<class TChar, CharacterEncoding TEncoding>
		struct CharCompareByValue
		{
		};

		template<class TChar>
		struct CharCompareByValue<TChar, CharacterEncoding::kByte>
		{
			static std::strong_ordering CompareOrdered(const TChar *str1, const TChar *str2, size_t len);
			static bool CompareEqual(const TChar *str1, const TChar *str2, size_t len);
		};

		template<>
		struct CharCompareByValue<Utf8Char_t, CharacterEncoding::kUTF8>
		{
			static std::strong_ordering CompareOrdered(const Utf8Char_t *str1, const Utf8Char_t *str2, size_t len);
			static bool CompareEqual(const Utf8Char_t *str1, const Utf8Char_t *str2, size_t len);
		};

		template<>
		struct CharCompareByValue<Utf16Char_t, CharacterEncoding::kUTF16>
		{
			static std::strong_ordering CompareOrdered(const Utf16Char_t *str1, const Utf16Char_t *str2, size_t len);
			static bool CompareEqual(const Utf16Char_t *str1, const Utf16Char_t *str2, size_t len);
		};

		template<>
		struct CharCompareByValue<char, CharacterEncoding::kASCII>
		{
			static std::strong_ordering CompareOrdered(const char *str1, const char *str2, size_t len);
			static bool CompareEqual(const char *str1, const char *str2, size_t len);
		};
	}

	template<class TChar, CharacterEncoding TEncoding>
	struct CharCompare
	{
		static std::strong_ordering CompareOrdered(const Span<const TChar> &str1, const Span<const TChar> &str2);
		static bool CompareEqual(const Span<const TChar> &str1, const Span<const TChar> &str2);
	};
}

#include <string.h>

#include "Algorithm.h"

namespace rkit
{
	template<class TChar, class TCanonicalChar, size_t TSizeOfTChar>
	std::strong_ordering priv::CharCompareAs<TChar, TCanonicalChar, TSizeOfTChar>::CompareOrdered(const TChar *str1, const TChar *str2, size_t len)
	{
		for (size_t i = 0; i < len; i++)
		{
			const TCanonicalChar cchar1 = static_cast<TCanonicalChar>(str1[i]);
			const TCanonicalChar cchar2 = static_cast<TCanonicalChar>(str2[i]);

			if (cchar1 != cchar2)
				return (cchar1 < cchar2) ? std::strong_ordering::less : std::strong_ordering::greater;
		}

		return std::strong_ordering::equal;
	}

	template<class TChar, class TCanonicalChar, size_t TSizeOfTChar>
	bool priv::CharCompareAs<TChar, TCanonicalChar, TSizeOfTChar>::CompareEqual(const TChar *str1, const TChar *str2, size_t len)
	{
		for (size_t i = 0; i < len; i++)
		{
			const TCanonicalChar cchar1 = static_cast<TCanonicalChar>(str1[i]);
			const TCanonicalChar cchar2 = static_cast<TCanonicalChar>(str2[i]);

			if (cchar1 != cchar2)
				return false;
		}

		return true;
	}

	template<class TChar>
	inline std::strong_ordering priv::CharCompareAs<TChar, uint8_t, 1>::CompareOrdered(const TChar *str1, const TChar *str2, size_t len)
	{
		return ::memcmp(str1, str2, len) <=> 0;
	}

	template<class TChar>
	inline bool priv::CharCompareAs<TChar, uint8_t, 1>::CompareEqual(const TChar *str1, const TChar *str2, size_t len)
	{
		return ::memcmp(str1, str2, len) == 0;
	}

	template<class TChar, CharacterEncoding TEncoding>
	inline std::strong_ordering CharCompare<TChar, TEncoding>::CompareOrdered(const Span<const TChar> &str1, const Span<const TChar> &str2)
	{
		const TChar *chars1 = str1.Ptr();
		const TChar *chars2 = str2.Ptr();

		const size_t len1 = str1.Count();
		const size_t len2 = str2.Count();

		const size_t shortLen = Min<size_t>(len1, len2);

		std::strong_ordering charComparison = priv::CharCompareByValue<TChar, TEncoding>::CompareOrdered(chars1, chars2, shortLen);

		if (charComparison != std::strong_ordering::equal)
			return charComparison;

		return len1 <=> len2;
	}

	template<class TChar, CharacterEncoding TEncoding>
	inline bool CharCompare<TChar, TEncoding>::CompareEqual(const Span<const TChar> &str1, const Span<const TChar> &str2)
	{
		const TChar *chars1 = str1.Ptr();
		const TChar *chars2 = str2.Ptr();

		const size_t len1 = str1.Count();
		const size_t len2 = str2.Count();

		if (len1 != len2)
			return false;

		return priv::CharCompareByValue<TChar, TEncoding>::CompareEqual(chars1, chars2, len1);
	}

	template<class TChar>
	std::strong_ordering priv::CharCompareByValue<TChar, CharacterEncoding::kByte>::CompareOrdered(const TChar *str1, const TChar *str2, size_t len)
	{
		return priv::CharCompareAs<TChar, TChar>::CompareOrdered(str1, str2, len);
	}

	template<class TChar>
	bool priv::CharCompareByValue<TChar, CharacterEncoding::kByte>::CompareEqual(const TChar *str1, const TChar *str2, size_t len)
	{
		return priv::CharCompareAs<TChar, TChar>::CompareEqual(str1, str2, len);
	}

	inline std::strong_ordering priv::CharCompareByValue<Utf8Char_t, CharacterEncoding::kUTF8>::CompareOrdered(const Utf8Char_t *str1, const Utf8Char_t *str2, size_t len)
	{
		return priv::CharCompareAs<Utf8Char_t, uint8_t>::CompareOrdered(str1, str2, len);
	}

	inline bool priv::CharCompareByValue<Utf8Char_t, CharacterEncoding::kUTF8>::CompareEqual(const Utf8Char_t *str1, const Utf8Char_t *str2, size_t len)
	{
		return priv::CharCompareAs<Utf8Char_t, uint8_t>::CompareEqual(str1, str2, len);
	}

	inline std::strong_ordering priv::CharCompareByValue<Utf16Char_t, CharacterEncoding::kUTF16>::CompareOrdered(const Utf16Char_t *str1, const Utf16Char_t *str2, size_t len)
	{
		return priv::CharCompareAs<Utf16Char_t, uint16_t>::CompareOrdered(str1, str2, len);
	}

	inline bool priv::CharCompareByValue<Utf16Char_t, CharacterEncoding::kUTF16>::CompareEqual(const Utf16Char_t *str1, const Utf16Char_t *str2, size_t len)
	{
		return priv::CharCompareAs<Utf16Char_t, uint16_t>::CompareEqual(str1, str2, len);
	}

	inline std::strong_ordering priv::CharCompareByValue<char, CharacterEncoding::kASCII>::CompareOrdered(const char *str1, const char *str2, size_t len)
	{
		return priv::CharCompareAs<char, uint8_t>::CompareOrdered(str1, str2, len);
	}

	inline bool priv::CharCompareByValue<char, CharacterEncoding::kASCII>::CompareEqual(const char *str1, const char *str2, size_t len)
	{
		return priv::CharCompareAs<char, uint8_t>::CompareEqual(str1, str2, len);
	}
}
