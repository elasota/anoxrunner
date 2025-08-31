#pragma once

#include "IncludeVulkan.h"

namespace rkit { namespace render {
	class MemoryRequirementsView;
} }

namespace rkit { namespace render { namespace vulkan {
	class VulkanDeviceBase;

	struct VulkanDeviceMemoryRequirements
	{
		const VulkanDeviceBase *m_device;
		VkMemoryRequirements m_memReqs;
	};

	MemoryRequirementsView ExportMemoryRequirements(const VulkanDeviceMemoryRequirements &memReqs);
} } }
