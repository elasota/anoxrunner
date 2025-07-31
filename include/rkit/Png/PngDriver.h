#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit
{
	namespace utils
	{
		struct IImage;
	}

	template<class T>
	class UniquePtr;

	struct ISeekableReadStream;
}

namespace rkit { namespace png {
	struct IPngDriver : public ICustomDriver
	{
		virtual Result LoadPNG(ISeekableReadStream &stream, UniquePtr<utils::IImage> &outImage) const = 0;
	};
} } // rkit::png
