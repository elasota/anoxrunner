#pragma once

namespace rkit
{
	struct Result;

	template<class T>
	class Span;
}

namespace rkit::render
{
	struct ISwapChainSyncPoint;
	struct BarrierGroup;
	struct DepthStencilTargetClear;
	struct RenderTargetClear;
	struct ImageRect2D;

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
	};
}
