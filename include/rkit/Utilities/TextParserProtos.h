#pragma once

#include <cstdint>

namespace rkit::utils
{
	enum class TextParserCommentType
	{
		kNone,
		kC,
		kBash,
	};

	enum class TextParserLexerType
	{
		kC,
		kSimple,
	};
}
