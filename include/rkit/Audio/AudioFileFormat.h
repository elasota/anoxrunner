#pragma once

#include <stdint.h>

namespace rkit::audio
{
	enum class AudioContainerFormat : uint32_t
	{
		kInvalid,

		kMPEGLayer3,
	};

	enum class AudioFileDataFormat : uint32_t
	{
		kInvalid,

		kMPEGLayer3,
	};
}
