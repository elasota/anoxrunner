#pragma once

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

	struct IBaseCommandQueue
	{
		virtual Result QueueWaitForOtherQueue(IBaseCommandQueue &otherQueue) = 0;
		virtual Result Submit(ICPUWaitableFence *cpuWaitableFence) = 0;
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

