#pragma once

#include <cstdint>

#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"

#include "rkit/Data/ContentID.h"

namespace anox { namespace data
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
		rkit::endian::LittleUInt32_t m_bitmap;
		rkit::endian::LittleUInt32_t m_next;
		rkit::endian::LittleUInt32_t m_waitMSec;

		rkit::endian::LittleInt32_t m_xOffset;
		rkit::endian::LittleInt32_t m_yOffset;
	};

	struct MaterialBitmapDef
	{
		rkit::data::ContentID m_contentID;
	};

	struct MaterialHeader
	{
		static const uint32_t kExpectedMagic = RKIT_FOURCC('A', 'M', 'T', 'L');
		static const uint32_t kExpectedVersion = 1;

		rkit::endian::LittleUInt32_t m_magic;
		rkit::endian::LittleUInt32_t m_version;
		rkit::endian::LittleUInt32_t m_width;
		rkit::endian::LittleUInt32_t m_height;

		uint8_t m_bilinear;
		uint8_t m_materialType;
		uint8_t m_colorType;
		uint8_t m_unused;

		rkit::endian::LittleUInt32_t m_numBitmaps;
		rkit::endian::LittleUInt32_t m_numFrames;

		// MaterialBitmapDef m_bitmapDefs
		// MaterialFrameDef m_frameDefs
	};
} } // anox::data
