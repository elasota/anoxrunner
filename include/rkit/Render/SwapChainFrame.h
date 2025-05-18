#pragma once

#include <cstddef>

namespace rkit { namespace render
{
	struct ISwapChainFrame
	{
	};

	struct ISwapChainSyncPoint
	{
		virtual ~ISwapChainSyncPoint() {}

		virtual size_t GetFrameIndex() const = 0;
	};
} } // rkit::render
