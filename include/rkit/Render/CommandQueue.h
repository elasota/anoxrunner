#pragma once

#include "Fence.h"
#include "PipelineStage.h"

namespace rkit
{
	struct Result;

	template<class T>
	class Span;

	template<class T>
	class EnumMask;
}

namespace rkit::render
{
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;
	struct ICPUWaitableFence;
	struct IInternalCommandQueue;
	struct IBinaryGPUWaitableFence;
	struct IBinaryCPUWaitableFence;

	struct IInternalCommandQueue
	{
		virtual ~IInternalCommandQueue() {}
	};

	struct IBaseCommandQueue
	{
		virtual Result QueueSignalBinaryGPUWaitable(IBinaryGPUWaitableFence &fence) = 0;
		virtual Result QueueSignalBinaryCPUWaitable(IBinaryCPUWaitableFence &fence) = 0;
		virtual Result QueueWaitForBinaryGPUWaitable(IBinaryGPUWaitableFence &fence, const EnumMask<PipelineStage> &stagesToWaitFor) = 0;

		virtual Result Flush() = 0;

		virtual IInternalCommandQueue *ToInternalCommandQueue() = 0;

		inline const IInternalCommandQueue *ToInternalCommandQueue() const
		{
			return const_cast<IBaseCommandQueue *>(this)->ToInternalCommandQueue();
		}
	};

	struct ICopyCommandQueue : public IBaseCommandQueue
	{
		virtual Result QueueCopy(const Span<ICopyCommandList*> &cmdLists) = 0;
		Result QueueCopy(ICopyCommandList &cmdList);
	};

	struct IComputeCommandQueue : public virtual ICopyCommandQueue
	{
		virtual Result QueueCompute(const Span<IComputeCommandList *> &cmdLists) = 0;
		Result QueueCompute(IComputeCommandList &cmdList);
	};

	struct IGraphicsCommandQueue : public virtual ICopyCommandQueue
	{
		virtual Result QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists) = 0;
		Result QueueGraphics(IGraphicsCommandList &cmdList);
	};

	struct IGraphicsComputeCommandQueue : public IComputeCommandQueue, public IGraphicsCommandQueue
	{
		virtual Result QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists) = 0;
		Result QueueGraphicsCompute(IGraphicsComputeCommandList &cmdList);
	};
}

#include "rkit/Core/Result.h"

