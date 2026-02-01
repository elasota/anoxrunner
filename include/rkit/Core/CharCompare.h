#pragma once

#include "CoreDefs.h"
#include "CharacterEncoding.h"
#include "Ordering.h"
#include "Span.h"

namespace rkit
{
	namespace priv
	{
		template<class TChar, class TCanonicalChar, size_t TSizeOfTChar = sizeof(TChar)>
		struct CharCompareAs
		{
			static Ordering Compare(const TChar *str1, const TChar *str2, size_t len);
		};

		template<class TChar>
		struct CharCompareAs<TChar, uint8_t, 1>
		{
			static Ordering Compare(const TChar *str1, const TChar *str2, size_t len);
		};

		template<class TChar, CharacterEncoding TEncoding>
		struct CharCompareByValue
		{
		};

		template<class TChar>
		struct CharCompareByValue<TChar, CharacterEncoding::kByte>
		{
			static Ordering Compare(const TChar *str1, const TChar *str2, size_t len);
		};

		template<>
		struct CharCompareByValue<Utf8Char_t, CharacterEncoding::kUTF8>
		{
			static Ordering Compare(const Utf8Char_t *str1, const Utf8Char_t *str2, size_t len);
		};

		template<>
		struct CharCompareByValue<Utf16Char_t, CharacterEncoding::kUTF16>
		{
			static Ordering Compare(const Utf16Char_t *str1, const Utf16Char_t *str2, size_t len);
		};

		template<>
		struct CharCompareByValue<char, CharacterEncoding::kASCII>
		{
			static Ordering Compare(const char *str1, const char *str2, size_t len);
		};
	}

	template<class TChar, CharacterEncoding TEncoding>
	struct CharCompare
	{
		static Ordering Compare(const Span<const TChar> &str1, const Span<const TChar> &str2);
	};
}

#include <string.h>

#include "Algorithm.h"

namespace rkit
{
	template<class TChar, class TCanonicalChar, size_t TSizeOfTChar>
	Ordering priv::CharCompareAs<TChar, TCanonicalChar, TSizeOfTChar>::Compare(const TChar *str1, const TChar *str2, size_t len)
	{
		for (size_t i = 0; i < len; i++)
		{
			const TCanonicalChar cchar1 = static_cast<TCanonicalChar>(str1[i]);
			const TCanonicalChar cchar2 = static_cast<TCanonicalChar>(str2[i]);

			if (cchar1 != cchar2)
				return (cchar1 < cchar2) ? Ordering::kLess : Ordering::kGreater;
		}

		return Ordering::kEqual;
	}

	template<class TChar>
	inline Ordering priv::CharCompareAs<TChar, uint8_t, 1>::Compare(const TChar *str1, const TChar *str2, size_t len)
	{
		int order = ::memcmp(str1, str2, len);
		if (order < 0)
			return Ordering::kLess;
		if (order > 0)
			return Ordering::kGreater;
		return Ordering::kEqual;
	}

	template<class TChar, CharacterEncoding TEncoding>
	inline Ordering CharCompare<TChar, TEncoding>::Compare(const Span<const TChar> &str1, const Span<const TChar> &str2)
	{
		const TChar *chars1 = str1.Ptr();
		const TChar *chars2 = str2.Ptr();

		const size_t len1 = str1.Count();
		const size_t len2 = str2.Count();

		const size_t shortLen = Min<size_t>(len1, len2);

		Ordering charComparison = priv::CharCompareByValue<TChar, TEncoding>::Compare(chars1, chars2, shortLen);

		if (charComparison != Ordering::kEqual)
			return charComparison;

		if (len1 < len2)
			return Ordering::kLess;
		else if (len1 > len2)
			return Ordering::kGreater;
		else
			return Ordering::kEqual;
	}

	template<class TChar>
	Ordering priv::CharCompareByValue<TChar, CharacterEncoding::kByte>::Compare(const TChar *str1, const TChar *str2, size_t len)
	{
		return priv::CharCompareAs<TChar, TChar>::Compare(str1, str2, len);
	}

	inline Ordering priv::CharCompareByValue<Utf8Char_t, CharacterEncoding::kUTF8>::Compare(const Utf8Char_t *str1, const Utf8Char_t *str2, size_t len)
	{
		return priv::CharCompareAs<Utf8Char_t, uint8_t>::Compare(str1, str2, len);
	}

	inline Ordering priv::CharCompareByValue<Utf16Char_t, CharacterEncoding::kUTF16>::Compare(const Utf16Char_t *str1, const Utf16Char_t *str2, size_t len)
	{
		return priv::CharCompareAs<Utf16Char_t, uint16_t>::Compare(str1, str2, len);
	}

	inline Ordering priv::CharCompareByValue<char, CharacterEncoding::kASCII>::Compare(const char *str1, const char *str2, size_t len)
	{
		return priv::CharCompareAs<char, uint8_t>::Compare(str1, str2, len);
	}
}
