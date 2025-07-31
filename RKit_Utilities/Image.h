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

		static Result Create(const utils::ImageSpec &spec, UniquePtr<ImageBase> &image);
	};
} } // rkit::utils
