#pragma once

#include "rkit/Core/StreamProtos.h"

#include "rkit/Render/RenderEnums.h"

#include "CommandQueueType.h"

namespace rkit
{
	struct ISeekableReadStream;
	struct Result;

	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;

	template<class T, class TUserdata>
	class CallbackSpan;
}

namespace rkit::data
{
	struct IRenderDataPackage;
}

namespace rkit::render
{
	struct IBaseCommandQueue;
	struct ICopyCommandQueue;
	struct IComputeCommandQueue;
	struct IGraphicsCommandQueue;
	struct IGraphicsComputeCommandQueue;
	struct ICPUWaitableFence;
	struct IPipelineLibraryLoader;
	struct IPipelineLibraryConfigValidator;
	struct IRenderDeviceCaps;
	struct ISwapChain;
	struct IDisplay;

	struct IRenderDevice
	{
		virtual ~IRenderDevice() {}

		virtual CallbackSpan<ICopyCommandQueue *, const void *> GetCopyQueues() const = 0;
		virtual CallbackSpan<IComputeCommandQueue *, const void *> GetComputeQueues() const = 0;
		virtual CallbackSpan<IGraphicsCommandQueue *, const void *> GetGraphicsQueues() const = 0;
		virtual CallbackSpan<IGraphicsComputeCommandQueue *, const void *> GetGraphicsComputeQueues() const = 0;

		virtual Result CreateCPUWaitableFence(UniquePtr<ICPUWaitableFence> &outFence) = 0;

		virtual const IRenderDeviceCaps &GetCaps() const = 0;

		virtual Result CreatePipelineLibraryLoader(UniquePtr<IPipelineLibraryLoader> &outLoader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart) = 0;

		virtual Result CreateSwapChain(UniquePtr<ISwapChain> &outSwapChain, IDisplay &display, uint8_t numBackBuffers, RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, IBaseCommandQueue &commandQueue) = 0;
	};
}
