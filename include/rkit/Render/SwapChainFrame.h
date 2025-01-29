#pragma once

#include <cstddef>

namespace rkit::render
{
	struct ISwapChainSubframe
	{
	};

	struct ISwapChainFrame
	{
		virtual ISwapChainSubframe *GetSubframe(size_t subframeIndex) const = 0;
	};

	struct ISwapChainSyncPoint
	{
		virtual ~ISwapChainSyncPoint() {}

		virtual size_t GetFrameIndex() const = 0;
	};
}
