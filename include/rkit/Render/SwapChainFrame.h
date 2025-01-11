#pragma once

namespace rkit::render
{
	struct ISwapChainSubframe
	{
	};

	struct ISwapChainFrame
	{
		virtual ISwapChainSubframe *GetSubframe(size_t index) const = 0;
	};
}
