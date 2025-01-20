#pragma once

#include <cstddef>

namespace rkit::render
{
	struct ISwapChainSubframe
	{
	};

	struct ISwapChainSyncPoint
	{
		virtual ~ISwapChainSyncPoint() {}

		virtual ISwapChainSubframe *GetSubframe(size_t subframeIndex) = 0;
		virtual Result WaitForCompletion() = 0;
	};
}
