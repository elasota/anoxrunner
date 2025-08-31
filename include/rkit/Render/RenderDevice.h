#pragma once

#include "rkit/Core/StreamProtos.h"

#include "rkit/Render/RenderEnums.h"
#include "rkit/Render/Fence.h"
#include "rkit/Render/PipelineLibraryItemProtos.h"

#include "CommandQueueType.h"

namespace rkit
{
	struct ISeekableReadStream;

	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;

	template<class T>
	class Span;

	template<class T, class TUserdata>
	class CallbackSpan;

	template<class T>
	class EnumMask;

	namespace data
	{
		struct IRenderDataPackage;
	}
}

namespace rkit { namespace render
{
	struct IBaseCommandQueue;
	struct ICopyCommandQueue;
	struct IComputeCommandQueue;
	struct IGraphicsCommandQueue;
	struct IGraphicsComputeCommandQueue;
	struct ICommandList;
	struct IBinaryCPUWaitableFence;
	struct IBinaryGPUWaitableFence;
	struct IPipelineLibraryLoader;
	struct IPipelineLibraryConfigValidator;
	struct IRenderDeviceCaps;
	struct ISwapChainPrototype;
	struct ISwapChain;
	struct ITexturePrototype;
	struct TextureSpec;
	struct IDisplay;
	struct ICopyCommandAllocator;
	struct IComputeCommandAllocator;
	struct IGraphicsCommandAllocator;
	struct IGraphicsComputeCommandAllocator;
	struct ISwapChainSyncPoint;
	struct RenderPassResources;
	struct IRenderPassInstance;

	struct IRenderDevice
	{
		virtual ~IRenderDevice() {}

		virtual CallbackSpan<ICopyCommandQueue *, const void *> GetCopyQueues() const = 0;
		virtual CallbackSpan<IComputeCommandQueue *, const void *> GetComputeQueues() const = 0;
		virtual CallbackSpan<IGraphicsCommandQueue *, const void *> GetGraphicsQueues() const = 0;
		virtual CallbackSpan<IGraphicsComputeCommandQueue *, const void *> GetGraphicsComputeQueues() const = 0;

		virtual Result CreateBinaryCPUWaitableFence(UniquePtr<IBinaryCPUWaitableFence> &outFence, bool startSignaled) = 0;
		virtual Result CreateBinaryGPUWaitableFence(UniquePtr<IBinaryGPUWaitableFence> &outFence) = 0;
		virtual Result CreateSwapChainSyncPoint(UniquePtr<ISwapChainSyncPoint> &outSyncPoint) = 0;

		virtual Result CreateRenderPassInstance(UniquePtr<IRenderPassInstance> &outInstance, const RenderPassRef_t &renderPass, const RenderPassResources &resources) = 0;

		virtual Result ResetBinaryFences(const ISpan<IBinaryCPUWaitableFence *> &fences) = 0;
		virtual Result WaitForBinaryFences(const ISpan<IBinaryCPUWaitableFence *> &fences, bool waitForAll) = 0;
		virtual Result WaitForBinaryFencesTimed(const ISpan<IBinaryCPUWaitableFence *> &fences, bool waitForAll, uint64_t timeoutMSec) = 0;
		virtual Result WaitForDeviceIdle() = 0;

		virtual const IRenderDeviceCaps &GetCaps() const = 0;

		virtual Result CreatePipelineLibraryLoader(UniquePtr<IPipelineLibraryLoader> &outLoader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart) = 0;

		virtual Result CreateSwapChainPrototype(UniquePtr<ISwapChainPrototype> &outSwapChainPrototype, IDisplay &display) = 0;
		virtual Result CreateSwapChain(UniquePtr<ISwapChain> &outSwapChain, UniquePtr<ISwapChainPrototype> &&prototype, uint8_t numImages, RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, IBaseCommandQueue &commandQueue) = 0;

		virtual Result CreateTexturePrototype(UniquePtr<ITexturePrototype> &outTexturePrototype, const TextureSpec &textureSpec,
			const EnumMask<TextureUsageFlag> &usage, const Span<IBaseCommandQueue * const> &restrictedQueues) = 0;
	};
} } // rkit::render
