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
		kMissing,

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
	};

	struct MaterialBitmapDef
	{
		rkit::data::ContentID m_contentID;
	};

	enum class MaterialInterformMovement : uint8_t
	{
		kNone,
		kScroll,
		kWander,
	};

	struct MaterialInterformHalf
	{
		data::MaterialInterformMovement m_movement = data::MaterialInterformMovement::kNone;
		rkit::endian::LittleUInt32_t m_bitmap;
		rkit::endian::LittleFloat32_t m_vx;
		rkit::endian::LittleFloat32_t m_vy;
		rkit::endian::LittleFloat32_t m_rate;
		rkit::endian::LittleFloat32_t m_strength;
		rkit::endian::LittleFloat32_t m_speed;
	};

	struct MaterialInterformData
	{
		MaterialInterformHalf m_halves[2];
		rkit::endian::LittleUInt32_t m_paletteBitmap;
	};

	struct MaterialHeader
	{
		static const uint32_t kExpectedMagic = RKIT_FOURCC('A', 'M', 'T', 'L');
		static const uint32_t kExpectedVersion = 1;

		rkit::endian::BigUInt32_t m_magic;
		rkit::endian::LittleUInt32_t m_version;
		rkit::endian::LittleUInt32_t m_width;
		rkit::endian::LittleUInt32_t m_height;

		uint8_t m_bilinear = 0;
		uint8_t m_materialType = 0;
		uint8_t m_colorType = 0;
		uint8_t m_unused = 0;

		rkit::endian::LittleUInt32_t m_numBitmaps;
		rkit::endian::LittleUInt32_t m_numFrames;

		// MaterialBitmapDef m_bitmapDefs
		// MaterialFrameDef m_frameDefs
		// if (m_materialType == MaterialType::kInterform) MaterialInterformData m_interform
	};
} } // anox::data
