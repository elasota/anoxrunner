#pragma once

#include "CommandQueueType.h"
#include "Fence.h"
#include "PipelineStage.h"

#include "rkit/Core/DynamicCastable.h"

namespace rkit
{
	struct Result;

	template<class T>
	class Span;

	template<class T>
	class UniquePtr;

	template<class T>
	class EnumMask;
}

namespace rkit::render
{
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;
	struct IInternalCommandQueue;
	struct IBinaryGPUWaitableFence;
	struct IBinaryCPUWaitableFence;
	struct ISwapChainSyncPoint;

	struct ICopyCommandAllocator;
	struct IGraphicsCommandAllocator;
	struct IComputeCommandAllocator;
	struct IGraphicsComputeCommandAllocator;

	struct ICopyCommandQueue;
	struct IComputeCommandQueue;
	struct IGraphicsCommandQueue;
	struct IGraphicsComputeCommandQueue;

	struct IInternalCommandQueue
	{
		virtual ~IInternalCommandQueue() {}
	};

	struct IBaseCommandQueue : public IDynamicCastable<ICopyCommandQueue, IGraphicsCommandQueue, IComputeCommandQueue, IGraphicsComputeCommandQueue>
	{
		virtual Result QueueSignalBinaryGPUWaitable(IBinaryGPUWaitableFence &fence) = 0;
		virtual Result QueueSignalBinaryCPUWaitable(IBinaryCPUWaitableFence &fence) = 0;
		virtual Result QueueWaitForBinaryGPUWaitable(IBinaryGPUWaitableFence &fence, const EnumMask<PipelineStage> &stagesToWaitFor) = 0;

		virtual CommandQueueType GetCommandQueueType() const = 0;

		virtual Result Flush() = 0;

		virtual ICopyCommandQueue *ToCopyCommandQueue() = 0;
		virtual IComputeCommandQueue *ToComputeCommandQueue() = 0;
		virtual IGraphicsCommandQueue *ToGraphicsCommandQueue() = 0;
		virtual IGraphicsComputeCommandQueue *ToGraphicsComputeCommandQueue() = 0;
		virtual IInternalCommandQueue *ToInternalCommandQueue() = 0;

		inline const IInternalCommandQueue *ToInternalCommandQueue() const
		{
			return const_cast<IBaseCommandQueue *>(this)->ToInternalCommandQueue();
		}
	};

	struct ICopyCommandQueue : public IBaseCommandQueue
	{
		virtual Result CreateCopyCommandAllocator(UniquePtr<ICopyCommandAllocator> &outCommandAllocator, bool isBundle) = 0;
	};

	struct IComputeCommandQueue : public virtual ICopyCommandQueue
	{
		virtual Result CreateComputeCommandAllocator(UniquePtr<IComputeCommandAllocator> &outCommandAllocator, bool isBundle) = 0;
	};

	struct IGraphicsCommandQueue : public virtual ICopyCommandQueue
	{
		virtual Result CreateGraphicsCommandAllocator(UniquePtr<IGraphicsCommandAllocator> &outCommandAllocator, bool isBundle) = 0;
	};

	struct IGraphicsComputeCommandQueue : public IComputeCommandQueue, public IGraphicsCommandQueue
	{
		virtual Result CreateGraphicsComputeCommandAllocator(UniquePtr<IGraphicsComputeCommandAllocator> &outCommandAllocator, bool isBundle) = 0;
	};
}
