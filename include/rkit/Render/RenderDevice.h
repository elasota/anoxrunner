#pragma once

#include "CommandQueueType.h"

namespace rkit::render
{
	struct ICopyCommandQueue;
	struct IComputeCommandQueue;
	struct IGraphicsCommandQueue;
	struct IGraphicsComputeCommandQueue;

	struct IRenderDevice
	{
		virtual ~IRenderDevice() {}

		virtual ICopyCommandQueue *GetCopyQueue(size_t index) const = 0;
		virtual IComputeCommandQueue *GetComputeQueue(size_t index) const = 0;
		virtual IGraphicsCommandQueue *GetGraphicsQueue(size_t index) const = 0;
		virtual IGraphicsComputeCommandQueue *GetGraphicsComputeQueue(size_t index) const = 0;
	};
}
