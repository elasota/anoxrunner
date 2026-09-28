#pragma once

#include "rkit/Core/CoreDefs.h"

#include "PipelineStage.h"
#include "TimelinePoint.h"
#include "TimelineSignalType.h"

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit::render
{
	struct IBinaryGPUWaitableFence;
	struct ICopyCommandEncoder;
	struct IComputeCommandEncoder;
	struct IGraphicsCommandEncoder;

	struct ISwapChainSyncPoint;
	struct IRenderPassInstance;

	struct ITimelineFence;

	class TimelineSignalIntent;

	struct IBaseCommandBatch
	{
		virtual Result Submit() = 0;

		virtual Result AddWaitForFence(IBinaryGPUWaitableFence &fence, const PipelineStageMask_t &subsequentStageMask) = 0;
		virtual Result AddSignalFence(IBinaryGPUWaitableFence &fence) = 0;
		virtual Result AddWaitForTimelineFence(ITimelineFence &fence, const PipelineStageMask_t &subsequentStageMask, TimelinePoint_t value) = 0;
		virtual Result AddSignalTimelineFence(const TimelineSignalIntent &intent) = 0;

		Result CloseBatch(const TimelineSignalIntent &intent);

	protected:
		virtual Result CloseBatchInternal() = 0;
	};

	struct ICopyCommandBatch : public IBaseCommandBatch
	{
		virtual Result OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder) = 0;
	};

	struct IComputeCommandBatch : public ICopyCommandBatch
	{
		virtual Result OpenComputeCommandEncoder(IComputeCommandEncoder *&outCopyCommandEncoder) = 0;
	};

	struct IGraphicsCommandBatch : public ICopyCommandBatch
	{
		virtual Result OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outCopyCommandEncoder, IRenderPassInstance &rpi) = 0;
	};

	struct IGraphicsComputeCommandBatch : public IGraphicsCommandBatch, public IComputeCommandBatch
	{
	};
} // rkit::render

namespace rkit::render
{
	inline Result IBaseCommandBatch::CloseBatch(const TimelineSignalIntent &intent)
	{
		AddSignalTimelineFence(intent);
		CloseBatchInternal();
	}
}
