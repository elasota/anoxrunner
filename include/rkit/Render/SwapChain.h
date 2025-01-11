#pragma once

#include "RenderEnums.h"

#include <cstddef>

namespace rkit
{
}

namespace rkit::render
{
	struct ISwapChainFrame;

	struct ISwapChain
	{
		virtual ~ISwapChain() {}

		virtual void GetExtents(uint32_t &outWidth, uint32_t &outHeight) const = 0;
		virtual Result AcquireFrame(ISwapChainFrame *&outFrame) = 0;
		virtual Result Present() = 0;
	};
}
