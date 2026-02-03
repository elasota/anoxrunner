#include "TextParser.h"

#include "rkit/Core/Span.h"
#include "rkit/Core/NoCopy.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Vector.h"

#include <cstring>

namespace rkit { namespace utils
{
	struct TextParserLocation
	{
		size_t m_line = 0;
		size_t m_col = 0;
		size_t m_pos = 0;
		bool m_lastCharWasCR = false;
		bool m_lastCharWasBackslash = false;
	};

	class ParserCharReader
	{
	public:
		explicit ParserCharReader(const Span<const uint8_t> &chars);

		void SetAllowQuotes(bool allowQuotes);
		void SetAllowCEscapes(bool allowCEscapes);

		const TextParserLocation &GetLocation() const;

		void Reset(const TextParserLocation &loc);

		bool SkipOne();
		bool ReadOne(uint8_t &outChar);
		bool PeekOne(uint8_t &outChar, size_t distanceAhead);
		bool PeekOne(uint8_t &outChar);

		Span<const uint8_t> GetSpan(size_t start, size_t size) const;

	private:
		static const size_t kMaxBufferedChars = 2;

		void Advance(uint8_t charToConsume);

		TextParserLocation m_loc;

		Span<const uint8_t> m_chars;
		uint8_t m_charBuffer[kMaxBufferedChars];
		size_t m_numBufferedChars;

		size_t m_readPos;
		bool m_allowQuotes;
		bool m_allowCEscapes;
	};

	class TextParser final : public TextParserBase, public NoCopy
	{
	public:
		TextParser(const Span<const uint8_t> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType);

		Result SkipWhitespace(bool skipNewLines);
		Result ReadSimpleToken(Span<const uint8_t> &outSpan);
		Result ReadCToken(Span<const uint8_t> &outSpan);

		Result SetSimpleDelimiters(const Span<const uint8_t> &delimiters) override;

		Result SkipWhitespace() override;
		Result ReadToken(bool &haveToken, Span<const uint8_t> &outSpan) override;
		Result ReadToEndOfLine(Span<const uint8_t> &outSpan) override;

		void GetLocation(size_t &outLine, size_t &outCol) const override;

		Result RequireToken(Span<const uint8_t> &outSpan) override;
		Result ExpectToken(const Span<const uint8_t> &str) override;

	private:
		Result ReadCIdentifier();
		Result ReadCOctalNumber();
		Result ReadCDecimalNumber();
		Result ReadCHexOrOctalNumber();
		Result ReadCHexNumber();
		Result ReadCString();

		static bool IsAlphaChar(uint8_t c);
		static bool IsNumericChar(uint8_t c);
		static bool IsIdentifierChar(uint8_t c);

		utils::TextParserCommentType m_commentType;
		utils::TextParserLexerType m_lexType;
		Vector<uint8_t> m_simpleDelimiters;

		bool m_isInLineComment;
		bool m_isInCBlockComment;
		bool m_isInQuotedString;

		ParserCharReader m_charReader;
	};
} } // rkit::utils

namespace rkit { namespace utils
{
	ParserCharReader::ParserCharReader(const Span<const uint8_t> &chars)
		: m_chars(chars)
		, m_readPos(0)
		, m_charBuffer{}
		, m_numBufferedChars(0)
		, m_allowCEscapes(false)
		, m_allowQuotes(false)
	{
		m_loc.m_pos = 0;
		m_loc.m_col = 1;
		m_loc.m_line = 1;
		m_loc.m_lastCharWasCR = false;
		m_loc.m_lastCharWasBackslash = false;
	}

	void ParserCharReader::SetAllowQuotes(bool allowQuotes)
	{
		m_allowQuotes = allowQuotes;
	}

	void ParserCharReader::SetAllowCEscapes(bool allowCEscapes)
	{
		m_allowCEscapes = allowCEscapes;
	}

	const TextParserLocation &ParserCharReader::GetLocation() const
	{
		return m_loc;
	}

	void ParserCharReader::Reset(const TextParserLocation &loc)
	{
		m_loc = loc;
		m_readPos = loc.m_pos;
		m_numBufferedChars = 0;
	}

	bool ParserCharReader::SkipOne()
	{
		if (m_numBufferedChars > 0)
		{
			uint8_t c = m_charBuffer[--m_numBufferedChars];
			Advance(c);

			return true;
		}

		if (m_readPos == m_chars.Count())
			return false;

		uint8_t c = m_chars[m_readPos++];
		Advance(c);

		return true;
	}

	bool ParserCharReader::ReadOne(uint8_t &outChar)
	{
		if (m_numBufferedChars > 0)
		{
			uint8_t c = m_charBuffer[--m_numBufferedChars];
			Advance(c);

			outChar = c;
			return true;
		}

		if (m_readPos == m_chars.Count())
			return false;

		uint8_t c = m_chars[m_readPos++];
		Advance(c);

		outChar = c;
		return true;
	}

	bool ParserCharReader::PeekOne(uint8_t &outChar, size_t distanceAhead)
	{
		RKIT_ASSERT(distanceAhead < kMaxBufferedChars);

		while (m_numBufferedChars <= distanceAhead)
		{
			if (m_readPos == m_chars.Count())
				return false;

			m_charBuffer[m_numBufferedChars++] = m_chars[m_readPos++];
		}

		outChar = m_charBuffer[m_numBufferedChars - 1];
		return true;
	}

	bool ParserCharReader::PeekOne(uint8_t &outChar)
	{
		return PeekOne(outChar, 0);
	}

	void ParserCharReader::Advance(uint8_t charToConsume)
	{
		m_loc.m_pos++;

		if (charToConsume == '\r')
		{
			m_loc.m_lastCharWasCR = true;
			m_loc.m_col = 1;
			m_loc.m_line++;
		}
		else
		{
			if (charToConsume == '\n')
			{
				m_loc.m_col = 1;
				if (!m_loc.m_lastCharWasCR)
					m_loc.m_line++;
			}
			else
				m_loc.m_col++;

			m_loc.m_lastCharWasCR = false;
		}
	}

	Span<const uint8_t> ParserCharReader::GetSpan(size_t start, size_t size) const
	{
		return m_chars.SubSpan(start, size);
	}

	TextParser::TextParser(const Span<const uint8_t> &contents, TextParserCommentType commentType, TextParserLexerType lexType)
		: m_charReader(contents)
		, m_commentType(commentType)
		, m_lexType(lexType)
		, m_isInCBlockComment(false)
		, m_isInLineComment(false)
		, m_isInQuotedString(false)
	{
		if (lexType == TextParserLexerType::kC)
		{
			m_charReader.SetAllowCEscapes(true);
			m_charReader.SetAllowQuotes(true);
		}
	}

	Result TextParser::SkipWhitespace(bool skipNewLines)
	{
		uint8_t c;

		if (m_isInQuotedString)
			RKIT_RETURN_OK;

		for (;;)
		{
			if (!m_charReader.PeekOne(c))
			{
				if (m_isInQuotedString)
					return ResultCode::kTextParsingFailed;

				RKIT_RETURN_OK;
			}

			if (m_isInLineComment)
			{
				if (c == '\r' || c == '\n')
					m_isInLineComment = false;

				(void)m_charReader.SkipOne();
				continue;
			}

			if (m_isInCBlockComment && c == '*')
			{
				uint8_t c2;
				(void)m_charReader.SkipOne();
				bool secondRead = m_charReader.ReadOne(c2);

				if (secondRead == false || c2 == '/')
					m_isInCBlockComment = false;

				continue;
			}

			if (skipNewLines == false && (c == '\r' || c == '\n'))
				break;

			if (c >= '\0' && c <= ' ')
			{
				(void)m_charReader.SkipOne();
				continue;
			}

			if (m_commentType == TextParserCommentType::kC && c == '/')
			{
				uint8_t c2 = 0;
				if (m_charReader.PeekOne(c2, 1))
				{
					if (c2 == '*')
					{
						(void)m_charReader.SkipOne();
						(void)m_charReader.SkipOne();

						m_isInCBlockComment = true;
						continue;
					}
					else if (c2 == '/')
					{
						(void)m_charReader.SkipOne();
						(void)m_charReader.SkipOne();

						m_isInLineComment = true;
						continue;
					}
				}
			}

			if (m_commentType == TextParserCommentType::kBash && c == '#')
			{
				(void)m_charReader.SkipOne();

				m_isInLineComment = true;
				continue;
			}

			break;
		}

		RKIT_RETURN_OK;
	}

	Result TextParser::ReadSimpleToken(Span<const uint8_t> &outSpan)
	{
		size_t startLoc = m_charReader.GetLocation().m_pos;
		size_t endLoc = startLoc;

		Span<const uint8_t> simpleDelimiters = m_simpleDelimiters.ToSpan();

		for (;;)
		{
			uint8_t c;
			if (!m_charReader.PeekOne(c))
			{
				endLoc = m_charReader.GetLocation().m_pos;
				break;
			}

			endLoc = m_charReader.GetLocation().m_pos;

			bool isSimpleDelimiter = false;
			for (uint8_t sdc : simpleDelimiters)
			{
				if (sdc == c)
				{
					isSimpleDelimiter = true;
					break;
				}
			}

			if (isSimpleDelimiter)
			{
				if (endLoc == startLoc)
				{
					m_charReader.SkipOne();
					endLoc++;
				}
				break;
			}

			RKIT_CHECK(SkipWhitespace());
			if (endLoc != m_charReader.GetLocation().m_pos)
				break;

			m_charReader.SkipOne();
		}

		outSpan = m_charReader.GetSpan(startLoc, endLoc - startLoc);

		RKIT_RETURN_OK;
	}

	Result TextParser::ReadCToken(Span<const uint8_t> &outSpan)
	{
		size_t startLoc = m_charReader.GetLocation().m_pos;
		size_t endLoc = startLoc;

		uint8_t c;
		if (!m_charReader.ReadOne(c))
			return ResultCode::kTextParsingFailed;

		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
		{
			RKIT_CHECK(ReadCIdentifier());
		}
		else if (c >= '1' && c <= '9')
		{
			RKIT_CHECK(ReadCDecimalNumber());
		}
		else if (c == '0')
		{
			RKIT_CHECK(ReadCHexOrOctalNumber());
		}
		else if (c == '\"')
		{
			RKIT_CHECK(ReadCString());
		}
		else
		{
			if (c == '+')
			{
				uint8_t nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=' || nextChar == '+')
						m_charReader.SkipOne();
				}
			}
			else if (c == '-')
			{
				uint8_t nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=' || nextChar == '-' || nextChar == '>')
						m_charReader.SkipOne();
				}
			}
			else if (c == '!' || c == '~' || c == '/' || c == '*' || c == '=' || c == '^')
			{
				uint8_t nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=')
						m_charReader.SkipOne();
				}
			}
			else if (c == '|' || c == '&')
			{
				uint8_t nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=' || nextChar == c)
						m_charReader.SkipOne();
				}
			}
			else if (c == '<' || c == '>')
			{
				uint8_t nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=')
						m_charReader.SkipOne();
					else if (nextChar == c)
					{
						m_charReader.SkipOne();
						if (m_charReader.PeekOne(nextChar))
						{
							if (nextChar == '=')
								m_charReader.SkipOne();
						}
					}
				}
			}
			else if (c == '(' || c == ')' || c == '[' || c == ']' || c == '.' || c == '?' || c == ':' || c == ',' || c == '{' || c == '}')
			{
			}
			else
				return ResultCode::kTextParsingFailed;
		}

		endLoc = m_charReader.GetLocation().m_pos;

		outSpan = m_charReader.GetSpan(startLoc, endLoc - startLoc);

		RKIT_RETURN_OK;
	}


	Result TextParser::SetSimpleDelimiters(const Span<const uint8_t> &delimiters)
	{
		m_simpleDelimiters.Reset();
		RKIT_CHECK(m_simpleDelimiters.Reserve(delimiters.Count()));
		RKIT_CHECK(m_simpleDelimiters.Append(delimiters));

		RKIT_RETURN_OK;
	}

	Result TextParser::SkipWhitespace()
	{
		return SkipWhitespace(true);
	}

	Result TextParser::ReadToken(bool &haveToken, Span<const uint8_t> &outSpan)
	{
		RKIT_CHECK(SkipWhitespace());

		uint8_t c;
		if (!m_charReader.PeekOne(c))
		{
			haveToken = false;
			RKIT_RETURN_OK;
		}

		if (m_lexType == TextParserLexerType::kSimple)
		{
			haveToken = true;
			return ReadSimpleToken(outSpan);
		}
		else if (m_lexType == TextParserLexerType::kC)
		{
			haveToken = true;
			return ReadCToken(outSpan);
		}

		return ResultCode::kInternalError;
	}

	Result TextParser::ReadToEndOfLine(Span<const uint8_t> &outSpan)
	{
		size_t startPos = m_charReader.GetLocation().m_pos;

		uint8_t c;
		while (m_charReader.PeekOne(c))
		{
			if (c == '\r' || c == '\n')
				break;

			m_charReader.SkipOne();
		}

		size_t endPos = m_charReader.GetLocation().m_pos;

		outSpan = m_charReader.GetSpan(startPos, endPos - startPos);
		RKIT_RETURN_OK;
	}

	void TextParser::GetLocation(size_t &outLine, size_t &outCol) const
	{
		const TextParserLocation &loc = m_charReader.GetLocation();
		outLine = loc.m_line;
		outCol = loc.m_col;
	}

	Result TextParser::RequireToken(Span<const uint8_t> &outSpan)
	{
		bool haveToken = false;
		RKIT_CHECK(ReadToken(haveToken, outSpan));

		if (!haveToken)
		{
			rkit::log::Error(u8"Unexpected end of file");
			return ResultCode::kTextParsingFailed;
		}

		RKIT_RETURN_OK;
	}

	Result TextParser::ExpectToken(const Span<const uint8_t> &str)
	{
		size_t line = 0;
		size_t col = 0;
		GetLocation(line, col);

		Span<const uint8_t> span;

		RKIT_CHECK(RequireToken(span));

		if (!CompareSpansEqual(span, str))
		{
			rkit::log::ErrorFmt(u8"[{}:{}] Expected '{}'", line, col, StringView(reinterpret_cast<const Utf8Char_t *>(str.Ptr()), str.Count()));
			return ResultCode::kTextParsingFailed;
		}

		RKIT_RETURN_OK;
	}

	bool TextParser::IsAlphaChar(uint8_t c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	bool TextParser::IsNumericChar(uint8_t c)
	{
		return (c >= '0' && c <= '9');
	}

	bool TextParser::IsIdentifierChar(uint8_t c)
	{
		return IsAlphaChar(c) || IsNumericChar(c) || (c == '_');
	}

	Result TextParser::ReadCIdentifier()
	{
		for (;;)
		{
			uint8_t c = '\0';
			if (!m_charReader.PeekOne(c))
				RKIT_RETURN_OK;

			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			{
				m_charReader.SkipOne();
				continue;
			}
			else
				break;
		}

		RKIT_RETURN_OK;
	}

	Result TextParser::ReadCOctalNumber()
	{
		for (;;)
		{
			uint8_t c = '\0';
			if (!m_charReader.PeekOne(c))
				RKIT_RETURN_OK;

			if (c >= '0' && c <= '7')
			{
				m_charReader.SkipOne();
				continue;
			}
			else if (IsIdentifierChar(c))
				return ResultCode::kTextParsingFailed;
			else
				break;
		}

		RKIT_RETURN_OK;
	}

	Result TextParser::ReadCDecimalNumber()
	{
		for (;;)
		{
			uint8_t c = '\0';
			if (!m_charReader.PeekOne(c))
				RKIT_RETURN_OK;

			if (c >= '0' && c <= '9')
			{
				m_charReader.SkipOne();
				continue;
			}
			else if (IsIdentifierChar(c))
				return ResultCode::kTextParsingFailed;
			else
				break;
		}

		RKIT_RETURN_OK;
	}

	Result TextParser::ReadCHexOrOctalNumber()
	{
		uint8_t secondChar = '\0';
		if (!m_charReader.PeekOne(secondChar))
			RKIT_RETURN_OK;

		if (secondChar == 'x')
		{
			m_charReader.SkipOne();
			return ReadCHexNumber();
		}

		return ReadCOctalNumber();
	}

	Result TextParser::ReadCString()
	{
		// FIXME: Validate encoding here
		for (;;)
		{
			uint8_t c = '\0';

			if (!m_charReader.ReadOne(c))
				return ResultCode::kTextParsingFailed;

			if (c == '\"')
				break;

			if (c == '\\')
			{
				if (!m_charReader.ReadOne(c))
					return ResultCode::kTextParsingFailed;

				if (c == 't' || c == 'n' || c == 'r' || c == '\"' || c == '\'')
				{
				}
				else
					return ResultCode::kTextParsingFailed;
			}
		}

		RKIT_RETURN_OK;
	}

	Result TextParser::ReadCHexNumber()
	{
		uint8_t nextChar = '\0';

		while (m_charReader.PeekOne(nextChar))
		{
			if ((nextChar >= '0' && nextChar <= '9') || (nextChar >= 'A' && nextChar <= 'Z') || (nextChar >= 'a' && nextChar <= 'z') || nextChar == '_')
			{
				m_charReader.SkipOne();

				if (nextChar == '_' || (nextChar > 'F' && nextChar <= 'Z') || (nextChar > 'f' && nextChar <= 'z'))
					return ResultCode::kTextParsingFailed;
			}
			else
				break;
		}

		RKIT_RETURN_OK;
	}
} } // rkit::utils

rkit::Result rkit::utils::TextParserBase::Create(const Span<const uint8_t> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<TextParserBase> &outParser)
{
	RKIT_CHECK(New<TextParser>(outParser, contents, commentType, lexType));
	RKIT_RETURN_OK;
}
