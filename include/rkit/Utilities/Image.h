#pragma once

#include <stdint.h>

namespace rkit { namespace utils {
	enum class PixelPacking : uint8_t
	{
		kUInt8,

		kUnknown,
	};

	struct ImageSpec
	{
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint8_t m_numChannels = 4;
		PixelPacking m_pixelPacking = PixelPacking::kUInt8;
	};

	struct IImage
	{
		virtual ~IImage() {}

		virtual const ImageSpec &GetImageSpec() const = 0;
		virtual uint8_t GetPixelSizeBytes() const = 0;

		virtual const void *GetScanline(uint32_t row) const = 0;
		virtual void *ModifyScanline(uint32_t row) = 0;

		inline uint32_t GetWidth() const { return GetImageSpec().m_width; }
		inline uint32_t GetHeight() const { return GetImageSpec().m_height; }

		inline uint8_t GetNumChannels() const { return GetImageSpec().m_numChannels; }
		inline PixelPacking GetPixelPacking() const { return GetImageSpec().m_pixelPacking; }
	};
} } // rkit::utils
