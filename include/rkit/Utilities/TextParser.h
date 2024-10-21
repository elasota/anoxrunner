#pragma once

#include "TextParserProtos.h"

#include <cstdint>

namespace rkit
{
	template<class T>
	class Span;

	struct Result;
}

namespace rkit::utils
{
	struct ITextParser
	{
		virtual ~ITextParser() {}

		virtual Result SkipWhitespace() = 0;
		virtual Result ReadToken(bool &haveToken, Span<const char> &outSpan) = 0;
		virtual Result ReadToEndOfLine(Span<const char> &outSpan) = 0;

		virtual void GetLocation(size_t &outLine, size_t &outCol) const = 0;
	};
}
