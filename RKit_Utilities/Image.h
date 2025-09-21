#pragma once

#include "rkit/Utilities/Image.h"

#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit { namespace utils {
	class ImageBase : public IImage
	{
	public:
		virtual Result Initialize() = 0;

		static Result Create(UniquePtr<ImageBase> &image, const utils::ImageSpec &spec);
	};
} } // rkit::utils

namespace rkit { namespace utils { namespace img
{
	size_t BytesPerPixel(uint8_t numChannels, PixelPacking pixelPacking);
	void BlitScanline(IImage &dest, const IImage &src, size_t srcRow, size_t srcByteOffset, size_t destRow, size_t destByteOffset, size_t numBytes);
} } } // rkit::utils::img
