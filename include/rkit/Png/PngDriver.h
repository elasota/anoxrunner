#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit
{
	namespace utils
	{
		struct IImage;
		struct ImageSpec;
	}

	template<class T>
	class UniquePtr;

	struct ISeekableReadStream;
}

namespace rkit { namespace png {
	struct IPngDriver : public ICustomDriver
	{
		virtual Result LoadPNGMetadata(utils::ImageSpec &imageSpec, ISeekableReadStream &stream) const = 0;
		virtual Result LoadPNG(UniquePtr<utils::IImage> &outImage, ISeekableReadStream &stream) const = 0;
	};
} } // rkit::png
