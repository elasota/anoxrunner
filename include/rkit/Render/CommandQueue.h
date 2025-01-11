#pragma once

#include "Fence.h"

namespace rkit
{
	struct Result;

	template<class T>
	class Span;
}

namespace rkit::render
{
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;
	struct ICPUWaitableFence;
	struct IInternalCommandQueue;

	struct IInternalCommandQueue
	{
		virtual ~IInternalCommandQueue() {}
	};

	struct IBaseCommandQueue
	{
		virtual Result QueueSignalGPUWaitable(GPUWaitableFence_t &outFence) = 0;
		virtual Result QueueSignalCPUWaitable(CPUWaitableFence_t &outFence) = 0;
		virtual Result QueueWaitFor(const GPUWaitableFence_t &gpuFence) = 0;

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

