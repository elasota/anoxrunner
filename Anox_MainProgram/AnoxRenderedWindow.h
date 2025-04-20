#pragma once

#include "rkit/Core/CoreDefs.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit
{
	template<class T>
	class RCPtr;
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
	struct IGraphicsSubsystem;
	struct PerFramePerDisplayResources;

	class RenderedWindowResources
	{
	public:
		virtual ~RenderedWindowResources() {}
	};

	class RenderedWindowBase
	{
	public:
		virtual ~RenderedWindowBase() {}

		virtual rkit::render::IDisplay &GetDisplay() = 0;
		virtual rkit::render::ISwapChain *GetSwapChain() = 0;

		virtual rkit::Result BeginFrame(IGraphicsSubsystem &graphicsSubsystem) = 0;
		virtual rkit::Result EndFrame(IGraphicsSubsystem &graphicsSubsystem) = 0;

		virtual const rkit::RCPtr<PerFramePerDisplayResources> &GetCurrentFrameResources() const = 0;

		static rkit::Result Create(rkit::UniquePtr<RenderedWindowBase> &outWindow, rkit::UniquePtr<rkit::render::IDisplay> &&display, rkit::UniquePtr<rkit::render::ISwapChainPrototype> &&prototype, rkit::UniquePtr<RenderedWindowResources> &&resources, rkit::render::IRenderDevice *device, rkit::render::IBaseCommandQueue *swapChainQueue, uint8_t numSwapChainFrames, uint8_t numSyncPoints);
	};
}
