#include "VulkanMemoryHeap.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"

namespace rkit { namespace render { namespace vulkan {
	VulkanMemoryHeap::VulkanMemoryHeap(VulkanDeviceBase &device, VkDeviceMemory deviceMemory, GPUMemorySize_t size, bool isCoherent)
		: m_device(device)
		, m_memory(deviceMemory)
		, m_size(size)
		, m_cpuPtr(nullptr)
		, m_isCoherent(isCoherent)
	{
	}

	VulkanMemoryHeap::~VulkanMemoryHeap()
	{
		if (m_cpuPtr)
			m_device.GetDeviceAPI().vkUnmapMemory(m_device.GetDevice(), m_memory);

		m_device.GetDeviceAPI().vkFreeMemory(m_device.GetDevice(), m_memory, m_device.GetAllocCallbacks());
	}

	Result VulkanMemoryHeap::MapMemory()
	{
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkMapMemory(m_device.GetDevice(), m_memory, 0, VK_WHOLE_SIZE, 0, &m_cpuPtr));

		return ResultCode::kOK;
	}

	MemoryPosition VulkanMemoryHeap::GetStartPosition() const
	{
		return MemoryPosition(const_cast<VulkanMemoryHeap *>(this), 0);
	}

	GPUMemorySize_t VulkanMemoryHeap::GetSize() const
	{
		return m_size;
	}

	void *VulkanMemoryHeap::GetCPUPtr() const
	{
		return m_cpuPtr;
	}
} } }
