#pragma once

#include "rkit/Core/Result.h"

#include "ImagePlane.h"
#include "RenderEnums.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace rkit { namespace render
{
	struct BarrierGroup;
	struct BufferImageFootprint;
	struct DepthStencilTargetClear;
	struct IBinaryCPUWaitableFence;
	struct IBinaryGPUWaitableFence;
	struct IBufferResource;
	struct IImageResource;
	struct ISwapChainSyncPoint;
	struct ImageRect2D;
	struct ImageRect3D;
	struct RenderTargetClear;

	struct IBaseCommandEncoder
	{
	};

	struct IGraphicsCommandEncoder : public IBaseCommandEncoder
	{
		virtual Result WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &subsequentStages) = 0;
		virtual Result SignalSwapChainPresentReady(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &priorStages) = 0;
		virtual Result PipelineBarrier(const BarrierGroup &barrierGroup) = 0;
		virtual Result ClearTargets(const Span<const RenderTargetClear> &renderTargetClears, const DepthStencilTargetClear *depthStencilClear, const Span<const ImageRect2D> &rects) = 0;
	};

	struct IComputeCommandEncoder : public IBaseCommandEncoder
	{
	};

	struct ICopyCommandEncoder : public IBaseCommandEncoder
	{
		virtual Result PipelineBarrier(const BarrierGroup &barrierGroup) = 0;
		virtual Result CopyBufferToImage(IImageResource &imageResource, const ImageRect3D &destRect,
			IBufferResource &bufferResource, const BufferImageFootprint &bufferFootprint,
			ImageLayout imageLayout, uint32_t mipLevel, uint32_t arrayLayer, ImagePlane imagePlane
		) = 0;
		virtual Result CopyBufferToBuffer(IBufferResource &destResource, GPUMemoryOffset_t destOffset,
			IBufferResource &srcResource, GPUMemoryOffset_t srcOffset, GPUMemorySize_t size
		) = 0;
	};
} } // rkit::render
