#pragma once

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	struct ISwapChainSyncPoint;
	struct BarrierGroup;

	struct IBaseCommandEncoder
	{
	};

	struct IGraphicsCommandEncoder : public IBaseCommandEncoder
	{
		virtual Result WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &subsequentStages) = 0;
		virtual Result SignalSwapChainPresentReady(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &priorStages) = 0;
		virtual Result PipelineBarrier(const BarrierGroup &barrierGroup) = 0;
	};

	struct IComputeCommandEncoder : public IBaseCommandEncoder
	{
	};

	struct ICopyCommandEncoder : public IBaseCommandEncoder
	{
	};
}
