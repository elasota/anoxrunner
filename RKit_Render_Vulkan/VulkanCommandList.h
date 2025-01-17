#pragma once

#include "rkit/Render/CommandList.h"
#include "rkit/Render/CommandQueueType.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanCommandListBase : public IGraphicsComputeCommandList, public IInternalCommandList
	{
	public:
		virtual VkCommandBuffer GetCommandBuffer() const = 0;

		static Result Create(UniquePtr<VulkanCommandListBase> &outCommandList, VulkanDeviceBase &device, VkCommandPool commandPool, CommandQueueType queueType, bool isBundle);
	};
}
