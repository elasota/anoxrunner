#pragma once

#include "rkit/Core/RefCounted.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	class RenderVulkanDriver;
	struct VulkanInstanceAPI;

	class RenderVulkanPhysicalDevice final : public RefCounted
	{
	public:
		explicit RenderVulkanPhysicalDevice(VkPhysicalDevice physDevice);

		Result InitPhysicalDevice(const VulkanInstanceAPI &instAPI);

	private:
		VkPhysicalDevice m_physDevice;
		VkPhysicalDeviceProperties m_props;
	};
}
