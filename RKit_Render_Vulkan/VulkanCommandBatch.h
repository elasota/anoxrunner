#pragma once

#include "rkit/Render/CommandBatch.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	namespace render
	{
		struct IRenderPassInstance;
	}
}

namespace rkit { namespace render { namespace vulkan
{
	class VulkanDeviceBase;
	class VulkanQueueProxyBase;
	class VulkanCommandAllocatorBase;

	class VulkanCommandBatchBase : public IGraphicsComputeCommandBatch
	{
	public:
		virtual ~VulkanCommandBatchBase() {}

		virtual Result ResetCommandBatch() = 0;
		virtual Result OpenCommandBatch() = 0;

		static Result Create(UniquePtr<VulkanCommandBatchBase> &cmdBatch, VulkanDeviceBase &device, VulkanQueueProxyBase &queue, VulkanCommandAllocatorBase &cmdAlloc);
	};
} } } // rkit::render::vulkan
