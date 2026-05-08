#include "rkit/Core/CoreLib.h"

#include "rkit/Core/Format.h"
#include "rkit/Core/StringView.h"

#include "ryu/ryu.h"

namespace rkit::text
{
	template<class TChar, CharacterEncoding TEncoding>
	void FormatStringImpl(IFormatStringWriter<TChar> &writer, const BaseStringSliceView<TChar, TEncoding> &fmtRef, const FormatParameterList<TChar> &paramListRef)
	{
		const BaseStringSliceView<TChar, TEncoding> fmt = fmtRef;
		const FormatParameterList<TChar> paramList = paramListRef;

		const size_t fmtStringLength = fmt.Length();

		const size_t mul10Limit = paramListRef.Count() / 10u;

		size_t scanPos = 0;
		size_t contiguousStart = 0;
		size_t numUnindexedArgs = 0;
		for (;;)
		{
			if (scanPos == fmtStringLength || fmt[scanPos] == '{')
			{
				const size_t contiguousLength = scanPos - contiguousStart;
				if (contiguousLength > 0)
					writer.WriteChars(fmt.SubString(contiguousStart, contiguousLength).ToSpan());

				if (scanPos == fmtStringLength)
					break;

				scanPos++;
				bool isValid = true;
				bool isIndexed = false;

				size_t argIndex = 0;
				for (;;)
				{
					if (scanPos == fmtStringLength)
					{
						isValid = false;
						break;
					}

					const TChar nextCh = fmt[scanPos];
					if (nextCh == '}')
					{
						scanPos++;
						contiguousStart = scanPos;
						break;
					}
					else if (nextCh >= '0' && nextCh <= '9')
					{
						if (isValid)
						{
							isIndexed = true;
							if (argIndex > mul10Limit)
								isValid = false;
							else
							{
								argIndex *= 10u;

								const uint8_t digit = (nextCh - '0');
								if (digit >= paramList.Count() - argIndex)
									isValid = false;
								else
									argIndex += digit;
							}
						}
					}
					else
						isValid = false;

					scanPos++;
				}

				if (isValid && !isIndexed)
				{
					if (paramList.Count() <= numUnindexedArgs)
						isValid = false;
					else
					{
						argIndex = numUnindexedArgs;
						numUnindexedArgs++;
					}
				}

				if (!isValid)
				{
					const TChar invalidText[] = { '<', 'I', 'N', 'V', 'A', 'L', 'I', 'D', '>' };
					const size_t kNumInvalidChars = sizeof(invalidText) / sizeof(TChar);

					writer.WriteChars(ConstSpan<TChar>(invalidText, kNumInvalidChars));
				}
				else
				{
					const FormatParameter<TChar> &formatParam = paramList[argIndex];
					formatParam.m_formatCallback(writer, formatParam.m_dataPtr);
				}
			}
			else
				scanPos++;
		}
	}
}

namespace rkit::text::formatters
{
	void FormatSignedInt(IFormatStringWriter<Utf8Char_t> &writer, intmax_t value)
	{
		if (value == 0)
		{
			writer.WriteChars(ConstSpan<Utf8Char_t>(u8"0", 1));
			return;
		}

		const size_t kMaxChars = ((sizeof(value) * 8) + 2) / 3 + 1;
		Utf8Char_t outChars[kMaxChars];

		const Utf8Char_t *kDigits = u8"0123456789";

		size_t strStartPos = kMaxChars;

		if (value < 0)
		{
			while (value != 0)
			{
				const intmax_t remainder = (value % 10);
				value = value / 10;

				--strStartPos;
				outChars[strStartPos] = kDigits[-remainder];
			}
			--strStartPos;
			outChars[strStartPos] = '-';
		}
		else
		{
			while (value != 0)
			{
				const intmax_t remainder = (value % 10);
				value = value / 10;

				--strStartPos;
				outChars[strStartPos] = kDigits[remainder];
			}
		}

		writer.WriteChars(ConstSpan<Utf8Char_t>(outChars + strStartPos, kMaxChars - strStartPos));
	}

	void FormatUnsignedInt(IFormatStringWriter<Utf8Char_t> &writer, uintmax_t value)
	{
		if (value == 0)
		{
			writer.WriteChars(ConstSpan<Utf8Char_t>(u8"0", 1));
			return;
		}

		const size_t kMaxChars = ((sizeof(value) * 8) + 2) / 3;
		Utf8Char_t outChars[kMaxChars];

		const Utf8Char_t *kDigits = u8"0123456789";

		size_t strStartPos = kMaxChars;

		while (value != 0)
		{
			const uintmax_t remainder = (value % 10u);
			value = value / 10u;

			--strStartPos;
			outChars[strStartPos] = kDigits[remainder];
		}

		writer.WriteChars(ConstSpan<Utf8Char_t>(outChars + strStartPos, kMaxChars - strStartPos));
	}

	void FormatFloat(IFormatStringWriter<Utf8Char_t> &writer, float f)
	{
		Utf8Char_t floatChars[16];
		int nChars = f2s_buffered_n(f, ReinterpretUtf8CharToAnsiChar(floatChars));

		writer.WriteChars(ConstSpan<Utf8Char_t>(floatChars, static_cast<size_t>(nChars)));
	}

	void FormatDouble(IFormatStringWriter<Utf8Char_t> &writer, double f)
	{
		Utf8Char_t doubleChars[25];
		int nChars = d2s_buffered_n(f, ReinterpretUtf8CharToAnsiChar(doubleChars));

		writer.WriteChars(ConstSpan<Utf8Char_t>(doubleChars, static_cast<size_t>(nChars)));
	}

	void FormatCString(IFormatStringWriter<char> &writer, const char *str)
	{
		writer.WriteChars(ConstSpan<char>(str, strlen(str)));
	}

	void FormatUtf8String(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t *str)
	{
		writer.WriteChars(StringView::FromCString(str).ToSpan());
	}
}

namespace rkit::text
{
	void FormatUtf8String(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t *fmtChars, size_t numFmtChars, const FormatParameter<Utf8Char_t> *fmtParameters, size_t numFmtParameters)
	{
		FormatStringImpl(writer, StringSliceView(fmtChars, numFmtChars), ConstSpan<FormatParameter<Utf8Char_t>>(fmtParameters, numFmtParameters));
	}
}
