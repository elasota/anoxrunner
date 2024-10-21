#include "TextParser.h"

#include "rkit/Core/Span.h"
#include "rkit/Core/NoCopy.h"

namespace rkit::utils
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

		Result SkipWhitespace() override;
		Result ReadToken(bool &haveToken, Span<const char> &outSpan) override;
		Result ReadToEndOfLine(Span<const char> &outSpan) override;

		void GetLocation(size_t &outLine, size_t &outCol) const override;

	private:
		utils::TextParserCommentType m_commentType;
		utils::TextParserLexerType m_lexType;

		bool m_isInLineComment;
		bool m_isInCBlockComment;
		bool m_isInQuotedString;

		ParserCharReader m_charReader;
	};
}

namespace rkit::utils
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

		char c = m_chars[m_readPos];
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
				if (m_loc.m_lastCharWasCR)
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

		for (;;)
		{
			char c;
			if (!m_charReader.PeekOne(c))
				break;

			endLoc = m_charReader.GetLocation().m_pos;

			RKIT_CHECK(SkipWhitespace());
			if (endLoc != m_charReader.GetLocation().m_pos)
				break;

			m_charReader.SkipOne();
		}

		outSpan = m_charReader.GetSpan(startLoc, endLoc - startLoc);

		return ResultCode::kOK;
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

		return ResultCode::kNotYetImplemented;
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
}

rkit::Result rkit::utils::TextParserBase::Create(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<TextParserBase> &outParser)
{
	RKIT_CHECK(New<TextParser>(outParser, contents, commentType, lexType));
	return ResultCode::kOK;
}
