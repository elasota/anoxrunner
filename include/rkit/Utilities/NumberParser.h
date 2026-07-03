#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class Span;
}


namespace rkit::utils
{
	template<class T, class TChar>
	bool TryParseInteger(T &outNumber, rkit::Span<const TChar> chars, uint8_t radix);

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParseInteger(T &outNumber, rkit::BaseStringSliceView<TChar, TEncoding> str, uint8_t radix);

	template<class T, class TChar>
	bool TryParsePartialInteger(T &outNumber, size_t &outCharsConsumed, rkit::Span<const TChar> chars, uint8_t radix);

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParsePartialInteger(T &outNumber, size_t &outCharsConsumed, rkit::BaseStringSliceView<TChar, TEncoding> str, uint8_t radix);

	template<class T, class TChar>
	bool TryParseFloat(T &outNumber, rkit::Span<const TChar> chars);

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParseFloat(T &outNumber, rkit::BaseStringSliceView<TChar, TEncoding> str);

	template<class T, class TChar>
	bool TryParsePartialFloat(T &outNumber, size_t &outCharsConsumed, rkit::Span<const TChar> chars);

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParsePartialFloat(T &outNumber, size_t &outCharsConsumed, rkit::BaseStringSliceView<TChar, TEncoding> str);
}

#include "rkit/Core/CoreLib.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/String.h"

#include <type_traits>

namespace rkit::utils::priv
{
	template<class T>
	class NumberSpanParseReader
	{
	public:
		explicit NumberSpanParseReader(rkit::Span<const T> chars);

		static bool StaticGetChar(void *self, int32_t &outChar);
		static void StaticUnGetChar(void *self);

		size_t GetOffset() const;

	private:
		rkit::Span<const T> m_chars;
		size_t m_offset = 0;
	};
}

namespace rkit::utils::priv
{
	template<class T>
	NumberSpanParseReader<T>::NumberSpanParseReader(rkit::Span<const T> chars)
		: m_chars(chars)
	{
	}

	template<class T>
	bool NumberSpanParseReader<T>::StaticGetChar(void *self, int32_t &outChar)
	{
		NumberSpanParseReader<T> *selfTyped = static_cast<NumberSpanParseReader<T>*>(self);

		if (selfTyped->m_offset == selfTyped->m_chars)
			return false;

		outChar = static_cast<int32_t>(selfTyped->m_chars[selfTyped->m_offset++]);
		return true;
	}

	template<class T>
	void NumberSpanParseReader<T>::StaticUnGetChar(void *self)
	{
		static_cast<NumberSpanParseReader<T> *>(self)->m_offset--;
	}

	template<class T>
	size_t NumberSpanParseReader<T>::GetOffset() const
	{
		return m_offset;
	}
}

namespace rkit::utils
{
	template<class T, class TChar>
	bool TryParsePartialFloat(T &outNumber, size_t &outCharsConsumed, rkit::Span<const TChar> chars)
	{
		static_assert(!rkit::IsSameType<T, wchar_t>::kValue, "wchar_t is not allowed");
		static_assert(std::is_floating_point<T>::value, "Type must be floating point");

		using ParseType_t = typename std::conditional<sizeof(T) == 4, float, double>::type;

		ParseType_t result = 0;
		size_t len = chars.Count();
		bool succeeded = false;

		if constexpr (sizeof(TChar) == 1)
		{
			if constexpr (sizeof(T) == 4)
				succeeded = rkit::utils::CharsToFloat(result, reinterpret_cast<const uint8_t *>(chars.Ptr()), len);
			else if constexpr (sizeof(T) == 8)
				succeeded = rkit::utils::CharsToDouble(result, reinterpret_cast<const uint8_t *>(chars.Ptr()), len);
			else
				static_assert(false, "Invalid float type");
		}
		else
		{
			priv::NumberSpanParseReader<TChar> parser(chars);
			if constexpr (sizeof(T) == 4)
				succeeded = rkit::utils::CharsToFloatDynamic(result, &parser, priv::NumberSpanParseReader<TChar>::StaticGetChar, priv::NumberSpanParseReader<TChar>::StaticUnGetChar);
			else if constexpr (sizeof(T) == 8)
				succeeded = rkit::utils::CharsToDoubleDynamic(result, &parser, priv::NumberSpanParseReader<TChar>::StaticGetChar, priv::NumberSpanParseReader<TChar>::StaticUnGetChar);
			else
				static_assert(false, "Invalid float type");

			len = parser.GetOffset();
		}

		outCharsConsumed = len;

		if (!succeeded)
			return false;

		outNumber = static_cast<T>(result);

		return true;
	}

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParsePartialFloat(T &outNumber, size_t &outCharsConsumed, rkit::BaseStringSliceView<TChar, TEncoding> str)
	{
		return TryParsePartialFloat(outNumber, outCharsConsumed, str.ToSpan());
	}

	template<class T, class TChar>
	bool TryParseFloat(T &outNumber, rkit::Span<const TChar> chars)
	{
		size_t charsConsumed = 0;
		return TryParsePartialFloat(outNumber, charsConsumed, chars) && charsConsumed == chars.Count();
	}

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParseFloat(T &outNumber, rkit::BaseStringSliceView<TChar, TEncoding> str)
	{
		size_t charsConsumed = 0;
		return TryParsePartialFloat(outNumber, charsConsumed, str) && charsConsumed == str.Length();
	}

	// Integer
	template<class T, class TChar>
	bool TryParsePartialInteger(T &outNumber, size_t &outCharsConsumed, rkit::Span<const TChar> chars, uint8_t radix)
	{
		static_assert(!rkit::IsSameType<T, bool>::kValue, "bool is not allowed");
		static_assert(!rkit::IsSameType<T, wchar_t>::kValue, "wchar_t is not allowed");
		static_assert(std::is_integral<T>::value, "Type must be an integral type");

		using ParseType_t = typename std::conditional<std::is_unsigned<T>::value, uint64_t, int64_t>::type;
		static_assert(sizeof(T) <= sizeof(ParseType_t), "Type is too big");

		uint64_t result = 0;
		size_t len = chars.Count();
		bool succeeded = false;

		if constexpr (sizeof(TChar) == 1)
		{
			if constexpr (std::is_unsigned<T>::value)
				succeeded = rkit::utils::CharsToUInt64(result, radix, reinterpret_cast<const uint8_t *>(chars.Ptr()), len);
			else
				succeeded = rkit::utils::CharsToInt64(result, radix, reinterpret_cast<const uint8_t *>(chars.Ptr()), len);
		}
		else
		{
			priv::NumberSpanParseReader<TChar> parser(chars);
			if constexpr (std::is_unsigned<T>::value)
				succeeded = rkit::utils::CharsToUInt64Dynamic(result, radix, &parser, priv::NumberSpanParseReader<TChar>::StaticGetChar, priv::NumberSpanParseReader<TChar>::StaticUnGetChar);
			else
				succeeded = rkit::utils::CharsToInt64Dynamic(result, radix, &parser, priv::NumberSpanParseReader<TChar>::StaticGetChar, priv::NumberSpanParseReader<TChar>::StaticUnGetChar);

			len = parser.GetOffset();
		}

		outCharsConsumed = len;

		if (!succeeded)
			return false;

		if (result < std::numeric_limits<T>::min() || result > std::numeric_limits<T>::max())
			return false;

		outNumber = static_cast<T>(result);
		return true;
	}

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParsePartialInteger(T &outNumber, size_t &outCharsConsumed, rkit::BaseStringSliceView<TChar, TEncoding> str, uint8_t radix)
	{
		return TryParsePartialInteger(outNumber, outCharsConsumed, str.ToSpan(), radix);
	}

	template<class T, class TChar>
	bool TryParseInteger(T &outNumber, rkit::Span<const TChar> chars, uint8_t radix)
	{
		size_t charsConsumed = 0;
		return TryParsePartialInteger(outNumber, charsConsumed, chars, radix) && charsConsumed == chars.Count();
	}

	template<class T, class TChar, CharacterEncoding TEncoding>
	bool TryParseInteger(T &outNumber, rkit::BaseStringSliceView<TChar, TEncoding> str, uint8_t radix)
	{
		size_t charsConsumed = 0;
		return TryParsePartialInteger(outNumber, charsConsumed, str, radix) && charsConsumed == str.Length();
	}
}
