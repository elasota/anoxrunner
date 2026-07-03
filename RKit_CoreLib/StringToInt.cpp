#include "rkit/Core/CoreLib.h"

#include <type_traits>
#include <limits>

namespace rkit::utils::priv
{
	class DynamicCharConverter
	{
	public:
		explicit DynamicCharConverter(uint8_t radix);

		uint8_t GetRadix() const;
		bool CharToDigit(int32_t ch, uint8_t &outDigit) const;

	private:
		uint8_t m_radix;
	};

	template<uint8_t TRadix>
	class StaticCharConverter
	{
	public:
		static constexpr uint8_t GetRadix();
		static constexpr bool CharToDigit(int32_t ch, uint8_t &outDigit);
	};

	class CharArrayReaderState
	{
	public:
		CharArrayReaderState(const uint8_t *chars, size_t size);

		bool GetOneChar(int32_t &outCh);
		void UnGetChar();

	private:
		const uint8_t *m_chars = nullptr;
		size_t m_offset = 0;
		size_t m_size = 0;
	};

	class CharCbReaderState
	{
	public:
		typedef bool (*GetOneCharCb_t)(void *state, int32_t &outChar);
		typedef void (*UnGetCharCb_t)(void *state);

		CharCbReaderState(void *state, GetOneCharCb_t getOneChar, UnGetCharCb_t unGetChar);

		bool GetOneChar(int32_t &outCh);
		void UnGetChar();

	private:
		void *m_state;
		GetOneCharCb_t m_getOneCharCb;
		UnGetCharCb_t m_unGetCharCb;
	};

	DynamicCharConverter::DynamicCharConverter(uint8_t radix)
		: m_radix(radix)
	{
	}

	uint8_t DynamicCharConverter::GetRadix() const
	{
		return m_radix;
	}

	bool DynamicCharConverter::CharToDigit(int32_t ch, uint8_t &outDigit) const
	{
		uint8_t digit = 0;
		if (StaticCharConverter<16>::CharToDigit(ch, digit) && digit < m_radix)
		{
			outDigit = digit;
			return true;
		}
		else
			return false;
	}

	template<uint8_t TRadix>
	constexpr uint8_t StaticCharConverter<TRadix>::GetRadix()
	{
		return TRadix;
	}

	template<uint8_t TRadix>
	constexpr bool StaticCharConverter<TRadix>::CharToDigit(int32_t ch, uint8_t &outDigit)
	{
		constexpr int32_t maxDecimalCh = (TRadix <= 10) ? TRadix : 10;

		if (ch >= '0' && ch <= ('0' + maxDecimalCh))
			outDigit = static_cast<uint8_t>(ch - '0');
		else
		{
			if constexpr (TRadix > 10)
			{
				if (ch >= 'a' && ch <= ('a' + (TRadix - 11)))
					outDigit = static_cast<uint8_t>(ch - 'a' + 10);
				else if (ch >= 'A' && ch <= ('A' + (TRadix - 11)))
					outDigit = static_cast<uint8_t>(ch - 'A' + 10);
				else
					return false;
			}
			else
				return false;
		}

		return true;
	}

	CharArrayReaderState::CharArrayReaderState(const uint8_t *chars, size_t size)
		: m_chars(chars)
		, m_size(size)
	{
	}

	bool CharArrayReaderState::GetOneChar(int32_t &outCh)
	{
		if (m_offset == m_size)
			return false;

		outCh = m_chars[m_offset++];
		return true;
	}

	void CharArrayReaderState::UnGetChar()
	{
		m_offset--;
	}

	CharCbReaderState::CharCbReaderState(void *state, GetOneCharCb_t getOneChar, UnGetCharCb_t unGetChar)
		: m_state(state)
		, m_getOneCharCb(getOneChar)
		, m_unGetCharCb(unGetChar)
	{
	}

	bool CharCbReaderState::GetOneChar(int32_t &outCh)
	{
		return m_getOneCharCb(m_state, outCh);
	}

	void CharCbReaderState::UnGetChar()
	{
		return m_unGetCharCb(m_state);
	}

	template<class TInt, class TState, class TCharConverter, TInt TMax>
	bool ParseCharsToUnsignedIntWithInitial(TInt &outResult, size_t &outCharsConsumed, TInt initialValue, size_t initialCharsConsumed, TState state, TCharConverter charConverter)
	{
		const TInt kMax = TMax;
		const TInt kRadix = charConverter.GetRadix();
		const TInt kMaxBeforeMultiply = kMax / kRadix;

		TInt result = initialValue;
		size_t charsConsumed = initialCharsConsumed;

		for (;;)
		{
			int32_t ch = 0;
			if (!state.GetOneChar(ch))
				break;

			uint8_t digit = 0;
			if (!charConverter.CharToDigit(ch, digit))
			{
				state.UnGetChar();
				break;
			}

			
			if (result > kMaxBeforeMultiply)
			{
				state.UnGetChar();
				outCharsConsumed = charsConsumed;
				return false;
			}

			result *= kRadix;

			if (kMax - result < digit)
			{
				state.UnGetChar();
				outCharsConsumed = charsConsumed;
				return false;
			}

			result += digit;
			charsConsumed++;
		}

		if (charsConsumed == 0)
			return false;

		outResult = result;
		outCharsConsumed = charsConsumed;
		return true;
	}

	template<class TInt, class TState, class TCharConverter, TInt TMax>
	bool ParseCharsToUnsignedInt(TInt &outResult, size_t &outCharsConsumed, TState state, TCharConverter charConverter)
	{
		return ParseCharsToUnsignedIntWithInitial<TInt, TState, TCharConverter, std::numeric_limits<TInt>::max()>(outResult, outCharsConsumed, 0, 0, state, charConverter);
	}

	template<class TInt, class TState, class TCharConverter>
	bool ParseCharsToSignedInt(TInt &outResult, size_t &outCharsConsumed, TState state, TCharConverter charConverter)
	{
		using UnsignedType_t = typename std::make_unsigned<TInt>::type;

		int32_t firstCh = 0;
		if (!state.GetOneChar(firstCh))
		{
			outCharsConsumed = 0;
			return false;
		}

		if (firstCh == '-')
		{
			constexpr UnsignedType_t kNegativeMax = static_cast<UnsignedType_t>(std::numeric_limits<TInt>::max()) + 1u;

			UnsignedType_t unsignedValue = 0;
			if (ParseCharsToUnsignedIntWithInitial<UnsignedType_t, TState, TCharConverter, kNegativeMax>(unsignedValue, outCharsConsumed, 0, 1, state, charConverter))
			{
				unsignedValue = static_cast<UnsignedType_t>(0) - unsignedValue;
				outResult = static_cast<TInt>(unsignedValue);
				return true;
			}
			else
				return false;
		}
		else
		{
			uint8_t firstDigit = 0;
			if (!charConverter.CharToDigit(firstCh, firstDigit))
			{
				state.UnGetChar();
				outCharsConsumed = 0;
				return false;
			}

			constexpr UnsignedType_t kPositiveMax = static_cast<UnsignedType_t>(std::numeric_limits<TInt>::max());

			UnsignedType_t unsignedValue = 0;
			if (ParseCharsToUnsignedIntWithInitial<UnsignedType_t, TState, TCharConverter, kPositiveMax>(unsignedValue, outCharsConsumed, firstDigit, 1, state, charConverter))
			{
				outResult = static_cast<TInt>(unsignedValue);
				return true;
			}
			else
				return false;
		}
	}

	template<class TInt, class TState, class TCharConverter>
	bool ParseCharsToIntWithConverter(TInt &outResult, size_t &outCharsConsumed, TState state, TCharConverter charConverter)
	{
		if constexpr (std::is_unsigned<TInt>::value)
			return ParseCharsToUnsignedInt<TInt, TState, TCharConverter, std::numeric_limits<TInt>::max()>(outResult, outCharsConsumed, state, charConverter);
		else
			return ParseCharsToSignedInt<TInt, TState, TCharConverter>(outResult, outCharsConsumed, state, charConverter);
	}

	template<class TInt, class TState>
	bool ParseCharsToIntWithState(TInt &outResult, uint8_t radix, size_t &outCharsConsumed, TState state)
	{
		if (radix == 10)
			return ParseCharsToIntWithConverter(outResult, outCharsConsumed, state, StaticCharConverter<10>());
		else if (radix == 16)
			return ParseCharsToIntWithConverter(outResult, outCharsConsumed, state, StaticCharConverter<16>());
		else
		{
			if (radix > 16)
				return false;

			return ParseCharsToIntWithConverter(outResult, outCharsConsumed, state, DynamicCharConverter(radix));
		}
	}

	template<class TInt>
	bool ParseCharsToInt(TInt &i, uint8_t radix, const uint8_t *chars, size_t &inOutLen)
	{
		return ParseCharsToIntWithState(i, radix, inOutLen, CharArrayReaderState(chars, inOutLen));
	}

	template<class TInt>
	bool ParseCharsToIntDynamic(TInt &i, uint8_t radix, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state))
	{
		size_t len = 0;
		return ParseCharsToIntWithState(i, radix, len, CharCbReaderState(state, getOneCharCb, unGetCharCb));
	}
}

bool RKIT_CORELIB_API rkit::utils::CharsToUInt64(uint64_t &i, uint8_t radix, const uint8_t *chars, size_t &inOutLen)
{
	return priv::ParseCharsToInt(i, radix, chars, inOutLen);
}

bool RKIT_CORELIB_API rkit::utils::CharsToInt64(int64_t &i, uint8_t radix, const uint8_t *chars, size_t &inOutLen)
{
	return priv::ParseCharsToInt(i, radix, chars, inOutLen);
}

bool RKIT_CORELIB_API rkit::utils::CharsToInt64Dynamic(int64_t &i, uint8_t radix, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state))
{
	return priv::ParseCharsToIntDynamic(i, radix, state, getOneCharCb, unGetCharCb);
}

bool RKIT_CORELIB_API rkit::utils::CharsToUInt64Dynamic(uint64_t &i, uint8_t radix, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state))
{
	return priv::ParseCharsToIntDynamic(i, radix, state, getOneCharCb, unGetCharCb);
}
