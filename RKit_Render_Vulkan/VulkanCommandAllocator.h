#pragma once

#include "rkit/Render/CommandQueueType.h"
#include "rkit/Render/CommandAllocator.h"

#include "IncludeVulkan.h"

#include <cstdint>

namespace rkit { namespace render { namespace vulkan
{
	class VulkanDeviceBase;
	class VulkanQueueProxyBase;

	class VulkanCommandAllocatorBase : public IGraphicsComputeCommandAllocator, public IInternalCommandAllocator
	{
	public:
		virtual CommandQueueType GetQueueType() const = 0;
		virtual bool IsBundle() const = 0;
		virtual VkCommandPool GetCommandPool() const = 0;
		virtual Result AcquireCommandBuffer(VkCommandBuffer &outCmdBuffer) = 0;

		static Result Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, VulkanQueueProxyBase &queue, CommandQueueType queueType, bool isBundle, uint32_t queueFamily);
	};
} } } // rkit::render::vulkan
