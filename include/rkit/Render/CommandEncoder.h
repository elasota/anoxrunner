#pragma once

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	struct ISwapChainSyncPoint;

	struct IBaseCommandEncoder
	{
	};

	struct IGraphicsCommandEncoder : public IBaseCommandEncoder
	{
		virtual Result WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &afterStages) = 0;
	};

	struct IComputeCommandEncoder : public IBaseCommandEncoder
	{
	};

	struct ICopyCommandEncoder : public IBaseCommandEncoder
	{
	};
}
