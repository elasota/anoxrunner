#include "VulkanPhysDevice.h"
#include "VulkanAPI.h"

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
		return ResultCode::kOK;
	}
}
