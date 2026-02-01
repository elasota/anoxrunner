#pragma once

#include "CoreDefs.h"
#include "CharacterEncoding.h"

#include <stdint.h>
#include <stddef.h>
#include <uchar.h>

namespace rkit
{
	template<class TChar, CharacterEncoding TEncoding, size_t TStaticSize = 16>
	class BaseString;

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringSliceView;

	template<class TChar, CharacterEncoding TEncoding>
	struct BaseStringSliceViewComparer;

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringView;

	template<class TChar, CharacterEncoding TEncoding>
	class BaseStringPoolBuilder;

	template<class TChar>
	class BaseStringConstructionBuffer;

	typedef BaseString<uint8_t, CharacterEncoding::kByte, 16> ByteString;
	typedef BaseString<char, CharacterEncoding::kASCII, 16> AsciiString;
	typedef BaseString<Utf8Char_t, CharacterEncoding::kUTF8, 16> String;
	typedef BaseString<Utf16Char_t, CharacterEncoding::kUTF16, 16> Utf16String;

	typedef BaseStringView<uint8_t, CharacterEncoding::kByte> ByteStringView;
	typedef BaseStringView<char, CharacterEncoding::kASCII> AsciiStringView;
	typedef BaseStringView<Utf8Char_t, CharacterEncoding::kUTF8> StringView;
	typedef BaseStringView<Utf16Char_t, CharacterEncoding::kUTF16> Utf16StringView;

	typedef BaseStringSliceView<uint8_t, CharacterEncoding::kByte> ByteStringSliceView;
	typedef BaseStringSliceView<char, CharacterEncoding::kASCII> AsciiStringSliceView;
	typedef BaseStringSliceView<Utf8Char_t, CharacterEncoding::kUTF8> StringSliceView;
	typedef BaseStringSliceView<Utf16Char_t, CharacterEncoding::kUTF16> Utf16StringSliceView;

	typedef BaseStringPoolBuilder<Utf8Char_t, CharacterEncoding::kUTF8> StringPoolBuilder;
	typedef BaseStringPoolBuilder<Utf16Char_t, CharacterEncoding::kUTF16> Utf16StringPoolBuilder;

	typedef BaseStringConstructionBuffer<uint8_t> ByteStringConstructionBuffer;
	typedef BaseStringConstructionBuffer<char> AsciiStringConstructionBuffer;
	typedef BaseStringConstructionBuffer<Utf8Char_t> StringConstructionBuffer;
	typedef BaseStringConstructionBuffer<Utf16Char_t> Utf16StringConstructionBuffer;
}
