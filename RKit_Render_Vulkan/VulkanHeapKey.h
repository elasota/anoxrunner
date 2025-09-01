#pragma once

#include <stdint.h>

namespace rkit { namespace render {
	class HeapKey;
} }

namespace rkit { namespace render { namespace vulkan {
	struct VulkanHeapKey
	{
		uint32_t m_memoryType;
		uint32_t m_cpuAccessible : 1;
		uint32_t m_unused : 31;
	};

	HeapKey EncodeHeapKey(const VulkanHeapKey &heapKey);
	VulkanHeapKey DecodeHeapKey(const HeapKey &heapKey);
} } }
