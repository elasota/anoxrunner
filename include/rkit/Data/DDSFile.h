#pragma once

#include "rkit/Core/FourCC.h"
#include "rkit/Core/Endian.h"

namespace rkit { namespace data
{
	namespace DDSFlags
	{
		enum Values
		{
			kCaps = 0x1,
			kHeight = 0x2,
			kWidth = 0x4,
			kPitch = 0x8,
			kPixelFormat = 0x1000,
			kMipMapCount = 0x20000,
			kLinearSize = 0x80000,
			kDepth = 0x800000,
		};
	}

	namespace DDSPixelFormatFlags
	{
		enum Values
		{
			kAlphaPixels = 0x1,
			kAlphaOnly = 0x2,
			kFourCC = 0x4,
			kRGB = 0x40,
			kYUV = 0x200,
			kLuminance = 0x20000,
		};
	}

	namespace DDSCaps
	{
		enum Values
		{
			kComplex = 0x8,	// Has more than one image
			kMipMap = 0x400000,
			kTexture = 0x1000,
		};
	}

	namespace DDSCaps2
	{
		enum Values
		{
			kCubeMap = 0x200,
			kCubeMapPositiveX = 0x400,
			kCubeMapNegativeX = 0x800,
			kCubeMapPositiveY = 0x1000,
			kCubeMapNegativeY = 0x2000,
			kCubeMapPositiveZ = 0x4000,
			kCubeMapNegativeZ = 0x8000,
			kVolume = 0x200000,
		};
	}

	enum class DDSResourceDimension
	{
		kTexture1D = 2,
		kTexture2D = 3,
		kTexture3D = 4,
	};

	namespace DDSExtendedMiscFlags
	{
		enum Values
		{
			kCubeMap = 0x4,
		};
	}

	namespace DDSExtendedMiscFlags2
	{
		enum Values
		{
			kAlphaModeUnknown = 0x0,
			kAlphaModeStraight = 0x1,
			kAlphaModePremultiplied = 0x2,
			kAlphaModeOpaque = 0x3,
			kAlphaModeCustom = 0x4,
		};
	}

	struct DDSPixelFormat
	{
		endian::LittleUInt32_t m_pixelFormatSize;
		endian::LittleUInt32_t m_pixelFormatFlags;
		endian::BigUInt32_t m_fourCC;
		endian::LittleUInt32_t m_rgbBitCount;
		endian::LittleUInt32_t m_rBitMask;
		endian::LittleUInt32_t m_gBitMask;
		endian::LittleUInt32_t m_bBitMask;
		endian::LittleUInt32_t m_aBitMask;
	};

	namespace DDSFourCCs
	{
		static const uint32_t kExtended = RKIT_FOURCC('D', 'X', '1', '0');
		static const uint32_t kBC1 = RKIT_FOURCC('D', 'X', 'T', '1');
		static const uint32_t kBC2 = RKIT_FOURCC('D', 'X', 'T', '3');
		static const uint32_t kBC3 = RKIT_FOURCC('D', 'X', 'T', '5');
	}

	struct DDSHeader
	{
		static const uint32_t kExpectedMagic = RKIT_FOURCC('D', 'D', 'S', ' ');

		endian::BigUInt32_t m_magic;
		endian::LittleUInt32_t m_headerSizeMinus4;
		endian::LittleUInt32_t m_ddsFlags;
		endian::LittleUInt32_t m_height;
		endian::LittleUInt32_t m_width;
		endian::LittleUInt32_t m_pitchOrLinearSize;
		endian::LittleUInt32_t m_depth;
		endian::LittleUInt32_t m_mipMapCount;
		endian::LittleUInt32_t m_reserved1[11];

		DDSPixelFormat m_pixelFormat;

		endian::LittleUInt32_t m_caps;
		endian::LittleUInt32_t m_caps2;
		endian::LittleUInt32_t m_caps3;
		endian::LittleUInt32_t m_caps4;

		endian::LittleUInt32_t m_reserved2;
	};

	struct DDSExtendedHeader
	{
		endian::LittleUInt32_t m_dxgiFormat;
		endian::LittleUInt32_t m_resourceDimension;
		endian::LittleUInt32_t m_miscFlags;
		endian::LittleUInt32_t m_arraySize;
		endian::LittleUInt32_t m_miscFlags2;
	};
} } // rkit::data
