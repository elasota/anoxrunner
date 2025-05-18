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
		virtual Result ReadToken(bool &haveToken, Span<const char> &outSpan) = 0;
		virtual Result ReadToEndOfLine(Span<const char> &outSpan) = 0;

		virtual void GetLocation(size_t &outLine, size_t &outCol) const = 0;

		virtual Result RequireToken(Span<const char> &outSpan) = 0;
		virtual Result ExpectToken(const StringView &str) = 0;
	};
} } // rkit::utils
