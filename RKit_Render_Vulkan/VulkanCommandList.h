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
	class VulkanCommandAllocatorBase;

	class VulkanCommandList final : public IGraphicsComputeCommandList, public IInternalCommandList
	{
	public:
		VulkanCommandList(VulkanDeviceBase &device, VkCommandBuffer commandBuffer, VulkanCommandAllocatorBase &allocator);
		~VulkanCommandList();

		IComputeCommandList *ToComputeCommandList() override;
		IGraphicsCommandList *ToGraphicsCommandList() override;
		IGraphicsComputeCommandList *ToGraphicsComputeCommandList() override;
		ICopyCommandList *ToCopyCommandList() override;

		IInternalCommandList *ToInternalCommandList() override;

		VkCommandBuffer GetCommandBuffer() const;

	private:
		VulkanDeviceBase &m_device;
		VulkanCommandAllocatorBase &m_allocator;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
	};
}
