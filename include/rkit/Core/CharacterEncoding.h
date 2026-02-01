#pragma once

#include "CoreDefs.h"

namespace rkit
{
	enum class CharacterEncoding
	{
		kASCII,
		kUTF8,
		kUTF16,
		kUTF32,
		kByte,

#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32
		kOSPath = kUTF16,
#else
		kOSPath = kUTF8,
#endif
	};

	inline unsigned char CharSizeForEncoding(CharacterEncoding encoding)
	{
		switch (encoding)
		{
		case CharacterEncoding::kASCII:
		case CharacterEncoding::kUTF8:
		case CharacterEncoding::kByte:
			return 1;
		case CharacterEncoding::kUTF16:
			return 2;
		case CharacterEncoding::kUTF32:
			return 4;
		default:
			return 0;
		}
	}

	inline bool IsCharacterEncodingCompatible(CharacterEncoding sourceEncoding, CharacterEncoding destEncoding)
	{
		if (sourceEncoding == destEncoding)
			return true;

		if (destEncoding == CharacterEncoding::kByte)
			return CharSizeForEncoding(sourceEncoding) == 1;

		if (sourceEncoding == CharacterEncoding::kASCII && destEncoding == CharacterEncoding::kUTF8)
			return true;

		return false;
	}
}
