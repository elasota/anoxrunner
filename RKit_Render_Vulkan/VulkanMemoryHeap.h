#pragma once

#include "rkit/Render/Memory.h"
#include "rkit/Render/MemoryProtos.h"

#include "IncludeVulkan.h"

namespace rkit { namespace render { namespace vulkan {
	class VulkanDeviceBase;

	class VulkanMemoryHeap final : public IMemoryHeap
	{
	public:
		VulkanMemoryHeap(VulkanDeviceBase &device, VkDeviceMemory deviceMemory, GPUMemorySize_t size, bool isCoherent);
		~VulkanMemoryHeap();

		VkDeviceMemory GetDeviceMemory() const;

		Result MapMemory();

		MemoryPosition GetStartPosition() const override;
		GPUMemorySize_t GetSize() const override;
		void *GetCPUPtr() const override;

	private:
		VulkanDeviceBase &m_device;
		VkDeviceMemory m_memory;
		GPUMemorySize_t m_size;
		bool m_isCoherent;
		void *m_cpuPtr;
	};
} } }

namespace rkit { namespace render { namespace vulkan {
	inline VkDeviceMemory VulkanMemoryHeap::GetDeviceMemory() const
	{
		return m_memory;
	}
} } }
