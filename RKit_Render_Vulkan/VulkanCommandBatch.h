#pragma once

#include "rkit/Render/CommandBatch.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;
	class VulkanCommandAllocatorBase;

	class VulkanCommandBatchBase : public IGraphicsComputeCommandBatch
	{
	public:
		virtual ~VulkanCommandBatchBase() {}

		virtual Result ClearCommandBatch() = 0;
		virtual Result OpenCommandBatch(bool cpuWaitable) = 0;

		static Result Create(UniquePtr<VulkanCommandBatchBase> &cmdBatch, VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc);
	};
};
