#pragma once

#include <cstdint>

#include "rkit/Core/FourCC.h"

namespace anox::data
{
	enum class MaterialType
	{
		kAnimation,
		kInterform,
		kWhiteNoise,
		kSingle,

		kCount,
	};

	enum class MaterialColorType
	{
		kLuminance,
		kLuminanceAlpha,
		kRGB,
		kRGBA,

		kCount,
	};

	struct MaterialFrameDef
	{
		uint32_t m_bitmap = 0;
		uint32_t m_next = 0;
		uint32_t m_waitMSec = 100;

		int32_t m_xOffset = 0;
		int32_t m_yOffset = 0;
	};

	struct MaterialBitmapDef
	{
		uint32_t m_nameIndex;
	};

	struct MaterialHeader
	{
		static const uint32_t kExpectedMagic = RKIT_FOURCC('A', 'M', 'T', 'L');

		uint32_t m_magic;
		uint32_t m_version;
		uint32_t m_width;
		uint32_t m_height;

		uint8_t m_bilinear;
		uint8_t m_materialType;
		uint8_t m_colorType;
		uint8_t m_unused;

		uint32_t m_numStrings;

		uint32_t m_numBitmaps;
		uint32_t m_numFrames;

		// uint32_t m_stringLengths[]
		// char m_stringChars[]
		// MaterialBitmapDef m_bitmapDefs
		// MaterialFrameDef m_frameDefs
	};
}
