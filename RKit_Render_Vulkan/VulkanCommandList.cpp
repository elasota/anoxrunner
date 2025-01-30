#include "VulkanCommandList.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanCommandAllocator.h"
#include "VulkanDevice.h"

#include "rkit/Core/NewDelete.h"

namespace rkit::render::vulkan
{

	VulkanCommandList::VulkanCommandList(VulkanDeviceBase &device, VkCommandBuffer commandBuffer, VulkanCommandAllocatorBase &allocator)
		: m_device(device)
		, m_cmdBuffer(commandBuffer)
		, m_allocator(allocator)
	{
	}

	VulkanCommandList::~VulkanCommandList()
	{
	}


	IComputeCommandList *VulkanCommandList::ToComputeCommandList()
	{
		switch (m_allocator.GetQueueType())
		{
		case CommandQueueType::kAsyncCompute:
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	IGraphicsCommandList *VulkanCommandList::ToGraphicsCommandList()
	{
		switch (m_allocator.GetQueueType())
		{
		case CommandQueueType::kGraphics:
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	IGraphicsComputeCommandList *VulkanCommandList::ToGraphicsComputeCommandList()
	{
		switch (m_allocator.GetQueueType())
		{
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	ICopyCommandList *VulkanCommandList::ToCopyCommandList()
	{
		switch (m_allocator.GetQueueType())
		{
		case CommandQueueType::kCopy:
		case CommandQueueType::kAsyncCompute:
		case CommandQueueType::kGraphics:
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	IInternalCommandList *VulkanCommandList::ToInternalCommandList()
	{
		return this;
	}

	VkCommandBuffer VulkanCommandList::GetCommandBuffer() const
	{
		return m_cmdBuffer;
	}
}
