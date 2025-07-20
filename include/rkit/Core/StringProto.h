#pragma once

#include "CharacterEncoding.h"

#include <cstddef>

namespace rkit
{
	template<class TChar, CharacterEncoding TEncoding, size_t TStaticSize = 16>
	class BaseString;

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringSliceView;

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringView;

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringPoolBuilder;

	template<class TChar>
	class BaseStringConstructionBuffer;

	typedef BaseString<char, CharacterEncoding::kASCII, 16> AsciiString;
	typedef BaseString<char, CharacterEncoding::kUTF8, 16> String;
	typedef BaseString<wchar_t, CharacterEncoding::kUnspecified, 16> WString;
	typedef BaseString<wchar_t, CharacterEncoding::kUTF16, 16> WString16;

	typedef BaseStringView<char, CharacterEncoding::kASCII> AsciiStringView;
	typedef BaseStringView<char, CharacterEncoding::kUTF8> StringView;
	typedef BaseStringView<wchar_t, CharacterEncoding::kUnspecified> WStringView;
	typedef BaseStringView<wchar_t, CharacterEncoding::kUTF16> WString16View;

	typedef BaseStringSliceView<char, CharacterEncoding::kASCII> AsciiStringSliceView;
	typedef BaseStringSliceView<char, CharacterEncoding::kUTF8> StringSliceView;
	typedef BaseStringSliceView<wchar_t, CharacterEncoding::kUnspecified> WStringSliceView;

	typedef BaseStringPoolBuilder<char, CharacterEncoding::kUTF8> StringPoolBuilder;
	typedef BaseStringPoolBuilder<wchar_t, CharacterEncoding::kUnspecified> WStringPoolBuilder;

	typedef BaseStringConstructionBuffer<char> StringConstructionBuffer;
	typedef BaseStringConstructionBuffer<wchar_t> WStringConstructionBuffer;
}
