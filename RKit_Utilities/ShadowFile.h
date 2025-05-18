#pragma once

#include "rkit/Utilities/ShadowFile.h"

namespace rkit
{
	struct ISeekableWriteStream;
}

namespace rkit { namespace utils
{
	class ShadowFileBase : public IShadowFile
	{
	public:
		virtual Result LoadShadowFile() = 0;
		virtual Result InitializeShadowFile() = 0;

		static Result Create(UniquePtr<ShadowFileBase> &shadowFile, ISeekableReadStream &readStream, ISeekableWriteStream *writeStream);
	};
} } // rkit::utils
