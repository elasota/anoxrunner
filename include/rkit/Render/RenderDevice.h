#pragma once

#include "rkit/Core/StreamProtos.h"

#include "rkit/Render/RenderEnums.h"
#include "rkit/Render/Fence.h"
#include "rkit/Render/MemoryProtos.h"
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
	struct IBinaryCPUWaitableFence;
	struct IBinaryGPUWaitableFence;
	struct IBufferPrototype;
	struct IBufferResource;
	struct IComputeCommandAllocator;
	struct ICopyCommandAllocator;
	struct ICopyCommandQueue;
	struct IComputeCommandQueue;
	struct IDisplay;
	struct IGraphicsCommandAllocator;
	struct IGraphicsCommandQueue;
	struct IGraphicsComputeCommandAllocator;
	struct IGraphicsComputeCommandQueue;
	struct IRenderPassInstance;
	struct ISwapChainSyncPoint;
	struct ICommandList;
	struct IImagePrototype;
	struct IImageResource;
	struct IMemoryHeap;
	struct IPipelineLibraryLoader;
	struct IPipelineLibraryConfigValidator;
	struct IRenderDeviceCaps;
	struct ISwapChainPrototype;
	struct ISwapChain;
	struct BufferSpec;
	struct BufferResourceSpec;
	struct ImageSpec;
	struct ImageResourceSpec;
	class HeapKey;
	class MemoryRegion;
	struct RenderPassResources;

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

		virtual Result CreateBufferPrototype(UniquePtr<IBufferPrototype> &outBufferPrototype, const BufferSpec &bufferSpec,
			const BufferResourceSpec &resourceSpec, const Span<IBaseCommandQueue *const> &restrictedQueues) = 0;
		virtual Result CreateBuffer(UniquePtr<IBufferResource> &outBuffer, UniquePtr<IBufferPrototype> &&bufferPrototype,
			const MemoryRegion &memRegion, const Span<const uint8_t> &initialData) = 0;

		virtual Result CreateImagePrototype(UniquePtr<IImagePrototype> &outImagePrototype, const ImageSpec &imageSpec,
			const ImageResourceSpec &resourceSpec, const Span<IBaseCommandQueue * const> &restrictedQueues) = 0;
		virtual Result CreateImage(UniquePtr<IImageResource> &outImage, UniquePtr<IImagePrototype> &&imagePrototype,
			const MemoryRegion &memRegion, const Span<const uint8_t> &initialData) = 0;

		virtual Result CreateMemoryHeap(UniquePtr<IMemoryHeap> &outHeap, const HeapKey &heapKey, GPUMemorySize_t size) = 0;

		virtual bool SupportsInitialTextureData() const = 0;
		virtual bool SupportsInitialBufferData() const = 0;

		Result CreateBufferPrototype(UniquePtr<IBufferPrototype> &outBufferPrototype, const BufferSpec &bufferSpec,
			const BufferResourceSpec &resourceSpec);
		Result CreateImagePrototype(UniquePtr<IImagePrototype> &outImagePrototype, const ImageSpec &imageSpec,
			const ImageResourceSpec &resourceSpec);
	};
} } // rkit::render

#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

namespace rkit { namespace render {
	inline Result IRenderDevice::CreateBufferPrototype(UniquePtr<IBufferPrototype> &outBufferPrototype, const BufferSpec &bufferSpec,
		const BufferResourceSpec &resourceSpec)
	{
		return this->CreateBufferPrototype(outBufferPrototype, bufferSpec, resourceSpec, Span<IBaseCommandQueue *const>());
	}

	inline Result IRenderDevice::CreateImagePrototype(UniquePtr<IImagePrototype> &outImagePrototype, const ImageSpec &imageSpec,
		const ImageResourceSpec &resourceSpec)
	{
		return this->CreateImagePrototype(outImagePrototype, imageSpec, resourceSpec, Span<IBaseCommandQueue *const>());
	}
} }
