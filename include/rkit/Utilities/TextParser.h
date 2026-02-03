#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"

#include "TextParserProtos.h"

#include <cstdint>

namespace rkit
{
	template<class T>
	class Span;
}

namespace rkit { namespace utils
{
	struct ITextParser
	{
		virtual ~ITextParser() {}

		virtual Result SkipWhitespace() = 0;
		virtual Result ReadToken(bool &haveToken, Span<const uint8_t> &outSpan) = 0;
		virtual Result ReadToEndOfLine(Span<const uint8_t> &outSpan) = 0;

		virtual void GetLocation(size_t &outLine, size_t &outCol) const = 0;

		virtual Result RequireToken(Span<const uint8_t> &outSpan) = 0;

		virtual Result ExpectToken(const Span<const uint8_t> &token) = 0;

		virtual Result SetSimpleDelimiters(const Span<const uint8_t> &delimiters) = 0;

	};

	template<class TChar, CharacterEncoding TEncoding>
	struct EncodingTextParserProxy
	{
		explicit EncodingTextParserProxy(ITextParser &parser);

		Result SkipWhitespace() const;
		Result ReadToken(bool &haveToken, Span<const TChar> &outSpan) const;
		Result ReadToEndOfLine(Span<const TChar> &outSpan) const;

		void GetLocation(size_t &outLine, size_t &outCol) const;

		Result RequireToken(Span<const TChar> &outSpan) const;

		Result ExpectToken(const BaseStringSliceView<TChar, TEncoding> &str) const;

		Result SetSimpleDelimiters(const BaseStringSliceView<TChar, TEncoding> &delimiters);

	private:
		ITextParser &m_parser;
	};
} } // rkit::utils

#include "rkit/Core/Result.h"

namespace rkit { namespace utils {
	template<class TChar, CharacterEncoding TEncoding>
	EncodingTextParserProxy<TChar, TEncoding>::EncodingTextParserProxy(ITextParser &parser)
		: m_parser(parser)
	{
		static_assert(sizeof(TChar) == 1, "EncodingTextParserProxy only works with 1-byte encodings");
	}

	template<class TChar, CharacterEncoding TEncoding>
	Result EncodingTextParserProxy<TChar, TEncoding>::SkipWhitespace() const
	{
		return m_parser.SkipWhitespace();
	}

	template<class TChar, CharacterEncoding TEncoding>
	Result EncodingTextParserProxy<TChar, TEncoding>::ReadToken(bool &haveToken, Span<const TChar> &outSpan) const
	{
		Span<const uint8_t> bytesSpan;
		RKIT_CHECK(m_parser.ReadToken(haveToken, bytesSpan));
		outSpan = bytesSpan.template ReinterpretCast<const TChar>();
		RKIT_RETURN_OK;
	}

	template<class TChar, CharacterEncoding TEncoding>
	Result EncodingTextParserProxy<TChar, TEncoding>::ReadToEndOfLine(Span<const TChar> &outSpan) const
	{
		Span<const uint8_t> bytesSpan;
		RKIT_CHECK(m_parser.ReadToEndOfLine(bytesSpan));
		outSpan = bytesSpan.template ReinterpretCast<const TChar>();
		RKIT_RETURN_OK;
	}

	template<class TChar, CharacterEncoding TEncoding>
	void EncodingTextParserProxy<TChar, TEncoding>::GetLocation(size_t &outLine, size_t &outCol) const
	{
		m_parser.GetLocation(outLine, outCol);
	}

	template<class TChar, CharacterEncoding TEncoding>
	Result EncodingTextParserProxy<TChar, TEncoding>::RequireToken(Span<const TChar> &outSpan) const
	{
		Span<const uint8_t> bytesSpan;
		RKIT_CHECK(m_parser.RequireToken(bytesSpan));
		outSpan = bytesSpan.template ReinterpretCast<const TChar>();
		RKIT_RETURN_OK;
	}

	template<class TChar, CharacterEncoding TEncoding>
	Result EncodingTextParserProxy<TChar, TEncoding>::ExpectToken(const BaseStringSliceView<TChar, TEncoding> &str) const
	{
		return m_parser.ExpectToken(str.ToSpan().template ReinterpretCast<const uint8_t>());
	}

	template<class TChar, CharacterEncoding TEncoding>
	Result EncodingTextParserProxy<TChar, TEncoding>::SetSimpleDelimiters(const BaseStringSliceView<TChar, TEncoding> &delimiters)
	{
		return m_parser.SetSimpleDelimiters(delimiters.ToSpan().template ReinterpretCast<const uint8_t>());
	}
} }
