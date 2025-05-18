#pragma once

#include <cstdint>

namespace rkit { namespace utils
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
} } // rkit::utils
