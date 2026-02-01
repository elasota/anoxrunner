#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/CharacterEncoding.h"

#if RKIT_CORELIB_DLL != 0
#	ifdef RKIT_CORELIB_INTERNAL
#		define RKIT_CORELIB_API RKIT_DLLEXPORT_API
#	else
#		define RKIT_CORELIB_API RKIT_DLLIMPORT_API
#	endif
#else
#	define RKIT_CORELIB_API
#endif


namespace rkit { namespace math {
	float RKIT_CORELIB_API SoftwareSqrtf(float f);
	double RKIT_CORELIB_API SoftwareSqrt(double f);
} }

namespace rkit { namespace text {
	enum class UnknownCharBehavior
	{
		kSkipInvalid,
		kReplaceInvalid,
		kFail,
	};

	uint32_t RKIT_CORELIB_API GetDefaultUnknownCharForEncoding(CharacterEncoding outEncoding);

	// Converts or measures output space for encoded characters
	// If outputChars is nullptr, then this outputs no characters, but can be used to measure required output capacity.
	// In such a scenario, outMaxChars still needs to be set.
	// If decode fails, the function returns 0.
	// If conversion succeeds, then the function returns the number of consumed characters.
	//
	// If decode fails, then the output contents are undefined.
	// The output and input buffers must not overlap.
	size_t RKIT_CORELIB_API ConvertText(void *outputChars, CharacterEncoding outEncoding, size_t outMaxChars, size_t &outCharsEmitted,
		const void *inputChars, CharacterEncoding inEncoding, size_t inCharCount, UnknownCharBehavior unknownCharBehavior, uint32_t unknownReplacementChar);
} }


namespace rkit { namespace utils {
	bool RKIT_CORELIB_API CharsToFloat(float &f, const uint8_t *chars, size_t &inOutLen);
	bool RKIT_CORELIB_API CharsToDouble(double &f, const uint8_t *chars, size_t &inOutLen);
	bool RKIT_CORELIB_API WCharsToFloat(float &f, const wchar_t *chars, size_t &inOutLen);
	bool RKIT_CORELIB_API WCharsToDouble(double &f, const wchar_t *chars, size_t &inOutLen);

	bool RKIT_CORELIB_API CharsToFloatDynamic(float &f, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state));
	bool RKIT_CORELIB_API CharsToDoubleDynamic(double &f, void *state, bool (*getOneCharCb)(void *state, int32_t &outChar), void (*unGetCharCb)(void *state));
} }
