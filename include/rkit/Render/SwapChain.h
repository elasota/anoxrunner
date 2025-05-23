#pragma once

#include "RenderEnums.h"

#include <cstddef>

namespace rkit { namespace render
{
	struct IBaseCommandQueue;
	struct ISwapChainSyncPoint;
	struct IRenderTargetView;
	struct IImageResource;

	struct ISwapChainPrototype
	{
		virtual ~ISwapChainPrototype() {}

		virtual Result CheckQueueCompatibility(bool &outIsCompatible, const IBaseCommandQueue &commandQueue) const = 0;
	};

	struct ISwapChain
	{
		virtual ~ISwapChain() {}

		virtual void GetExtents(uint32_t &outWidth, uint32_t &outHeight) const = 0;
		virtual Result AcquireFrame(ISwapChainSyncPoint &syncPoint) = 0;
		virtual Result Present(ISwapChainSyncPoint &syncPoint) = 0;

		virtual IRenderTargetView *GetRenderTargetViewForFrame(size_t frameIndex) = 0;
		virtual IImageResource *GetImageForFrame(size_t frameIndex) = 0;
	};
} } // rkit::render
