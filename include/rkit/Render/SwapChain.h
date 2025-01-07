#pragma once

#include "RenderEnums.h"

namespace rkit
{
}

namespace rkit::render
{
	struct ISwapChain
	{
		virtual ~ISwapChain() {}

		virtual void GetExtents(uint32_t &outWidth, uint32_t &outHeight) const = 0;
	};
}
