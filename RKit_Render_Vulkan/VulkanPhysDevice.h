#pragma once

#include "rkit/Render/CommandQueueType.h"

#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Vector.h"

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

		VkPhysicalDevice GetPhysDevice() const;

		void GetQueueTypeInfo(CommandQueueType queueType, uint32_t &outQueueFamilyIndex, uint32_t &outNumQueues) const;

	private:
		static const size_t kNumCommandQueueTypes = static_cast<size_t>(CommandQueueType::kCount);

		struct QueueFamilyInfo
		{
			uint32_t m_queueFamilyIndex = 0;
			uint32_t m_numQueues = 0;
		};

		VkPhysicalDevice m_physDevice;
		VkPhysicalDeviceProperties m_props;

		Vector<VkQueueFamilyProperties> m_queueFamilyProps;

		QueueFamilyInfo m_queueFamilyInfos[kNumCommandQueueTypes];
	};
}
