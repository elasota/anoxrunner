#include "VulkanHeapKey.h"

#include "rkit/Render/HeapKey.h"

namespace rkit { namespace render { namespace vulkan {

	HeapKey EncodeHeapKey(const VulkanHeapKey &vkHeapKey)
	{
		static_assert(sizeof(vkHeapKey) == sizeof(uint64_t), "Heap key size mismatch");

		uint64_t heapKeyBits = 0;
		memcpy(&heapKeyBits, &vkHeapKey, sizeof(heapKeyBits));

		return HeapKey(heapKeyBits);
	}

	VulkanHeapKey DecodeHeapKey(const HeapKey &heapKey)
	{
		static_assert(sizeof(VulkanHeapKey) == sizeof(uint64_t), "Heap key size mismatch");

		uint64_t heapKeyBits = heapKey.GetAsUInt64();

		VulkanHeapKey vkHeapKey = {};
		memcpy(&vkHeapKey, &heapKeyBits, sizeof(heapKeyBits));

		return vkHeapKey;
	}
} } }
