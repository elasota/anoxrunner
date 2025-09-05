#pragma once

#include "rkit/Core/CoreDefs.h"

#include "PipelineStage.h"

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit { namespace render
{
	struct IBinaryGPUWaitableFence;
	struct ICopyCommandEncoder;
	struct IComputeCommandEncoder;
	struct IGraphicsCommandEncoder;

	struct ISwapChainSyncPoint;
	struct IRenderPassInstance;

	struct IBaseCommandBatch
	{
		virtual Result Submit() = 0;
		virtual Result WaitForCompletion() = 0;
		virtual Result CloseBatch() = 0;

		virtual Result AddWaitForFence(IBinaryGPUWaitableFence &fence, const PipelineStageMask_t &subsequentStageMask) = 0;
		virtual Result AddSignalFence(IBinaryGPUWaitableFence &fence) = 0;
	};

	struct ICopyCommandBatch : public IBaseCommandBatch
	{
		virtual Result OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder) = 0;
	};

	struct IComputeCommandBatch : public virtual ICopyCommandBatch
	{
		virtual Result OpenComputeCommandEncoder(IComputeCommandEncoder *&outCopyCommandEncoder) = 0;
	};

	struct IGraphicsCommandBatch : public virtual ICopyCommandBatch
	{
		virtual Result OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outCopyCommandEncoder, IRenderPassInstance &rpi) = 0;
	};

	struct IGraphicsComputeCommandBatch : public IGraphicsCommandBatch, public IComputeCommandBatch
	{
	};
} } // rkit::render
