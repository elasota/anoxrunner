#pragma once

#include <cstddef>

namespace rkit::render
{
	struct ISwapChainFrame
	{
	};

	struct ISwapChainSyncPoint
	{
		virtual ~ISwapChainSyncPoint() {}

		virtual size_t GetFrameIndex() const = 0;
	};
}
