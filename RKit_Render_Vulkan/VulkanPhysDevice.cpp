#include "VulkanPhysDevice.h"
#include "VulkanAPI.h"
#include "VulkanCheck.h"

namespace rkit::render::vulkan
{
	RenderVulkanPhysicalDevice::RenderVulkanPhysicalDevice(VkPhysicalDevice physDevice)
		: m_physDevice(physDevice)
		, m_props{}
	{
	}

	Result RenderVulkanPhysicalDevice::InitPhysicalDevice(const VulkanInstanceAPI &instAPI)
	{
		instAPI.vkGetPhysicalDeviceProperties(m_physDevice, &m_props);

		uint32_t propertyCount = 0;
		instAPI.vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &propertyCount, nullptr);

		RKIT_CHECK(m_queueFamilyProps.Resize(propertyCount));
		instAPI.vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &propertyCount, m_queueFamilyProps.GetBuffer());

		for (uint32_t i = 0; i < propertyCount; i++)
		{
			VkQueueFlags graphicsComputeMask = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
			VkQueueFlags dmaMask = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

			const VkQueueFamilyProperties &qfProps = m_queueFamilyProps[i];
			VkQueueFlags qfFlags = qfProps.queueFlags;

			VkQueueFlags graphicsComputeBits = (qfFlags & graphicsComputeMask);

			CommandQueueType queueType = CommandQueueType::kCount;

			if (graphicsComputeBits == graphicsComputeMask)
				queueType = CommandQueueType::kGraphicsCompute;
			else if (graphicsComputeBits == VK_QUEUE_GRAPHICS_BIT)
				queueType = CommandQueueType::kGraphics;
			else if (graphicsComputeBits == VK_QUEUE_COMPUTE_BIT)
				queueType = CommandQueueType::kAsyncCompute;
			else if ((qfFlags & dmaMask) == VK_QUEUE_TRANSFER_BIT)
				queueType = CommandQueueType::kCopy;

			if (queueType != CommandQueueType::kCount)
			{
				QueueFamilyInfo &qfi = m_queueFamilyInfos[static_cast<size_t>(queueType)];
				qfi.m_numQueues = qfProps.queueCount;
				qfi.m_queueFamilyIndex = i;
			}
		}

		return ResultCode::kOK;
	}

	VkPhysicalDevice RenderVulkanPhysicalDevice::GetPhysDevice() const
	{
		return m_physDevice;
	}

	const VkPhysicalDeviceProperties &RenderVulkanPhysicalDevice::GetPhysDeviceProperties() const
	{
		return m_props;
	}

	void RenderVulkanPhysicalDevice::GetQueueTypeInfo(CommandQueueType queueType, uint32_t &outQueueFamilyIndex, uint32_t &outNumQueues) const
	{
		const QueueFamilyInfo &qfi = m_queueFamilyInfos[static_cast<size_t>(queueType)];
		outQueueFamilyIndex = qfi.m_queueFamilyIndex;
		outNumQueues = qfi.m_numQueues;
	}
}
