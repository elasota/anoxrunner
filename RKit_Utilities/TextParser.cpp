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
		explicit ParserCharReader(const Span<const char> &chars);

		void SetAllowQuotes(bool allowQuotes);
		void SetAllowCEscapes(bool allowCEscapes);

		const TextParserLocation &GetLocation() const;

		void Reset(const TextParserLocation &loc);

		bool SkipOne();
		bool ReadOne(char &outChar);
		bool PeekOne(char &outChar, size_t distanceAhead);
		bool PeekOne(char &outChar);

		Span<const char> GetSpan(size_t start, size_t size) const;

	private:
		static const size_t kMaxBufferedChars = 2;

		void Advance(char charToConsume);

		TextParserLocation m_loc;

		Span<const char> m_chars;
		char m_charBuffer[kMaxBufferedChars];
		size_t m_numBufferedChars;

		size_t m_readPos;
		bool m_allowQuotes;
		bool m_allowCEscapes;
	};

	class TextParser final : public TextParserBase, public NoCopy
	{
	public:
		TextParser(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType);

		Result SkipWhitespace(bool skipNewLines);
		Result ReadSimpleToken(Span<const char> &outSpan);
		Result ReadCToken(Span<const char> &outSpan);

		Result SetSimpleDelimiters(const Span<const char> &delimiters) override;

		Result SkipWhitespace() override;
		Result ReadToken(bool &haveToken, Span<const char> &outSpan) override;
		Result ReadToEndOfLine(Span<const char> &outSpan) override;

		void GetLocation(size_t &outLine, size_t &outCol) const override;

		Result RequireToken(Span<const char> &outSpan) override;
		Result ExpectToken(const StringView &str) override;

	private:
		Result ReadCIdentifier();
		Result ReadCOctalNumber();
		Result ReadCDecimalNumber();
		Result ReadCHexOrOctalNumber();
		Result ReadCHexNumber();
		Result ReadCString();

		static bool IsAlphaChar(char c);
		static bool IsNumericChar(char c);
		static bool IsIdentifierChar(char c);

		utils::TextParserCommentType m_commentType;
		utils::TextParserLexerType m_lexType;
		Vector<char> m_simpleDelimiters;

		bool m_isInLineComment;
		bool m_isInCBlockComment;
		bool m_isInQuotedString;

		ParserCharReader m_charReader;
	};
} } // rkit::utils

namespace rkit { namespace utils
{
	ParserCharReader::ParserCharReader(const Span<const char> &chars)
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
			char c = m_charBuffer[--m_numBufferedChars];
			Advance(c);

			return true;
		}

		if (m_readPos == m_chars.Count())
			return false;

		char c = m_chars[m_readPos++];
		Advance(c);

		return true;
	}

	bool ParserCharReader::ReadOne(char &outChar)
	{
		if (m_numBufferedChars > 0)
		{
			char c = m_charBuffer[--m_numBufferedChars];
			Advance(c);

			outChar = c;
			return true;
		}

		if (m_readPos == m_chars.Count())
			return false;

		char c = m_chars[m_readPos++];
		Advance(c);

		outChar = c;
		return true;
	}

	bool ParserCharReader::PeekOne(char &outChar, size_t distanceAhead)
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

	bool ParserCharReader::PeekOne(char &outChar)
	{
		return PeekOne(outChar, 0);
	}

	void ParserCharReader::Advance(char charToConsume)
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

	Span<const char> ParserCharReader::GetSpan(size_t start, size_t size) const
	{
		return m_chars.SubSpan(start, size);
	}

	TextParser::TextParser(const Span<const char> &contents, TextParserCommentType commentType, TextParserLexerType lexType)
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
		char c;

		if (m_isInQuotedString)
			return ResultCode::kOK;

		for (;;)
		{
			if (!m_charReader.PeekOne(c))
			{
				if (m_isInQuotedString)
					return ResultCode::kTextParsingFailed;

				return ResultCode::kOK;
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
				char c2;
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
				char c2 = 0;
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

		return ResultCode::kOK;
	}

	Result TextParser::ReadSimpleToken(Span<const char> &outSpan)
	{
		size_t startLoc = m_charReader.GetLocation().m_pos;
		size_t endLoc = startLoc;

		Span<const char> simpleDelimiters = m_simpleDelimiters.ToSpan();

		for (;;)
		{
			char c;
			if (!m_charReader.PeekOne(c))
			{
				endLoc = m_charReader.GetLocation().m_pos;
				break;
			}

			endLoc = m_charReader.GetLocation().m_pos;

			bool isSimpleDelimiter = false;
			for (char sdc : simpleDelimiters)
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

		return ResultCode::kOK;
	}

	Result TextParser::ReadCToken(Span<const char> &outSpan)
	{
		size_t startLoc = m_charReader.GetLocation().m_pos;
		size_t endLoc = startLoc;

		char c;
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
				char nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=' || nextChar == '+')
						m_charReader.SkipOne();
				}
			}
			else if (c == '-')
			{
				char nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=' || nextChar == '-' || nextChar == '>')
						m_charReader.SkipOne();
				}
			}
			else if (c == '!' || c == '~' || c == '/' || c == '*' || c == '=' || c == '^')
			{
				char nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=')
						m_charReader.SkipOne();
				}
			}
			else if (c == '|' || c == '&')
			{
				char nextChar = '\0';
				if (m_charReader.PeekOne(nextChar))
				{
					if (nextChar == '=' || nextChar == c)
						m_charReader.SkipOne();
				}
			}
			else if (c == '<' || c == '>')
			{
				char nextChar = '\0';
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

		return ResultCode::kOK;
	}


	Result TextParser::SetSimpleDelimiters(const Span<const char> &delimiters)
	{
		m_simpleDelimiters.Reset();
		RKIT_CHECK(m_simpleDelimiters.Reserve(delimiters.Count()));
		RKIT_CHECK(m_simpleDelimiters.Append(delimiters));

		return rkit::ResultCode::kOK;
	}

	Result TextParser::SkipWhitespace()
	{
		return SkipWhitespace(true);
	}

	Result TextParser::ReadToken(bool &haveToken, Span<const char> &outSpan)
	{
		RKIT_CHECK(SkipWhitespace());

		char c;
		if (!m_charReader.PeekOne(c))
		{
			haveToken = false;
			return ResultCode::kOK;
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

	Result TextParser::ReadToEndOfLine(Span<const char> &outSpan)
	{
		size_t startPos = m_charReader.GetLocation().m_pos;

		char c;
		while (m_charReader.PeekOne(c))
		{
			if (c == '\r' || c == '\n')
				break;

			m_charReader.SkipOne();
		}

		size_t endPos = m_charReader.GetLocation().m_pos;

		outSpan = m_charReader.GetSpan(startPos, endPos - startPos);
		return ResultCode::kOK;
	}

	void TextParser::GetLocation(size_t &outLine, size_t &outCol) const
	{
		const TextParserLocation &loc = m_charReader.GetLocation();
		outLine = loc.m_line;
		outCol = loc.m_col;
	}

	Result TextParser::RequireToken(Span<const char> &outSpan)
	{
		bool haveToken = false;
		RKIT_CHECK(ReadToken(haveToken, outSpan));

		if (!haveToken)
		{
			rkit::log::Error("Unexpected end of file");
			return ResultCode::kTextParsingFailed;
		}

		return ResultCode::kOK;
	}

	Result TextParser::ExpectToken(const StringView &str)
	{
		size_t line = 0;
		size_t col = 0;
		GetLocation(line, col);

		Span<const char> span;

		RKIT_CHECK(RequireToken(span));

		if (span.Count() != str.Length() || memcmp(span.Ptr(), str.GetChars(), span.Count()))
		{
			rkit::log::ErrorFmt("[%zu:%zu] Expected '%s'", line, col, str.GetChars());
			return ResultCode::kTextParsingFailed;
		}

		return ResultCode::kOK;
	}

	bool TextParser::IsAlphaChar(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	bool TextParser::IsNumericChar(char c)
	{
		return (c >= '0' && c <= '9');
	}

	bool TextParser::IsIdentifierChar(char c)
	{
		return IsAlphaChar(c) || IsNumericChar(c) || (c == '_');
	}

	Result TextParser::ReadCIdentifier()
	{
		for (;;)
		{
			char c = '\0';
			if (!m_charReader.PeekOne(c))
				return ResultCode::kOK;

			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
			{
				m_charReader.SkipOne();
				continue;
			}
			else
				break;
		}

		return ResultCode::kOK;
	}

	Result TextParser::ReadCOctalNumber()
	{
		for (;;)
		{
			char c = '\0';
			if (!m_charReader.PeekOne(c))
				return ResultCode::kOK;

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

		return ResultCode::kOK;
	}

	Result TextParser::ReadCDecimalNumber()
	{
		for (;;)
		{
			char c = '\0';
			if (!m_charReader.PeekOne(c))
				return ResultCode::kOK;

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

		return ResultCode::kOK;
	}

	Result TextParser::ReadCHexOrOctalNumber()
	{
		char secondChar = '\0';
		if (!m_charReader.PeekOne(secondChar))
			return ResultCode::kOK;

		if (secondChar == 'x')
		{
			m_charReader.SkipOne();
			return ReadCHexNumber();
		}

		return ReadCOctalNumber();
	}

	Result TextParser::ReadCString()
	{
		for (;;)
		{
			char c = '\0';

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

		return ResultCode::kOK;
	}

	Result TextParser::ReadCHexNumber()
	{
		char nextChar = '\0';

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

		return ResultCode::kOK;
	}
} } // rkit::utils

rkit::Result rkit::utils::TextParserBase::Create(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<TextParserBase> &outParser)
{
	RKIT_CHECK(New<TextParser>(outParser, contents, commentType, lexType));
	return ResultCode::kOK;
}
