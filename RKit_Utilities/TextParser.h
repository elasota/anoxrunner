#pragma once

#include "rkit/Utilities/TextParser.h"

namespace rkit
{
	template<class T>
	class Span;

	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	class TextParserBase : public ITextParser
	{
	public:
		static Result Create(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<TextParserBase> &outParser);
	};
}
