#pragma once

#include <cstdint>

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct ISwapChainPrototype;
	struct IBaseCommandQueue;
	struct IRenderDevice;
	struct ISwapChain;
	struct IDisplay;
}

namespace anox
{
	class RenderedWindowBase
	{
	public:
		virtual ~RenderedWindowBase() {}

		virtual rkit::render::IDisplay &GetDisplay() = 0;
		virtual rkit::render::ISwapChain *GetSwapChain() = 0;

		virtual rkit::Result BeginFrame() = 0;
		virtual rkit::Result EndFrame() = 0;

		static rkit::Result Create(rkit::UniquePtr<RenderedWindowBase> &outWindow, rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototype, rkit::render::IRenderDevice *device, rkit::render::IBaseCommandQueue *swapChainQueue, uint8_t numBackBuffers, uint8_t numSyncPoints);
	};
}
