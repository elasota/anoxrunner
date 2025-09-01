#include "VulkanMemoryRequirements.h"

#include "rkit/Math/BitOps.h"
#include "rkit/Render/HeapSpec.h"
#include "rkit/Render/Memory.h"

#include "VulkanDevice.h"
#include "VulkanHeapKey.h"

namespace rkit { namespace render { namespace vulkan {
	namespace VulkanMemoryRequirementsFuncs
	{
		rkit::Optional<HeapKey> HeapKeyFilter(const void *userdata, const HeapSpec &heapSpec)
		{
			const VulkanDeviceMemoryRequirements &deviceMemReqs = *static_cast<const VulkanDeviceMemoryRequirements *>(userdata);
			const VulkanDeviceBase &device = *deviceMemReqs.m_device;
			const VkMemoryRequirements &memReqs = deviceMemReqs.m_memReqs;

			const uint32_t highestMemTypePlusOne = bitops::FindHighestSet(memReqs.memoryTypeBits) + 1;

			rkit::Optional<uint32_t> bestMemType;
			for (uint32_t memType = 0; memType < highestMemTypePlusOne; memType++)
			{
				if ((memReqs.memoryTypeBits >> memType) & 1)
				{
					if (device.MemoryTypeMeetsHeapSpec(memType, heapSpec))
					{
						// Find the most-restrictive available memory type
						if (!bestMemType.IsSet() ||
								(
									device.MemoryTypeIsSubset(memType, bestMemType.Get())
									&& !device.MemoryTypeIsSubset(bestMemType.Get(), memType)
								)
							)
							bestMemType = memType;
					}
				}
			}

			if (!bestMemType.IsSet())
				return rkit::Optional<HeapKey>();

			VulkanHeapKey vkHeapKey = {};
			vkHeapKey.m_memoryType = bestMemType.Get();
			vkHeapKey.m_cpuAccessible = (heapSpec.m_cpuAccessible ? 1 : 0);

			return EncodeHeapKey(vkHeapKey);
		}
	}

	MemoryRequirementsView ExportMemoryRequirements(const VulkanDeviceMemoryRequirements &memReqs)
	{
		return MemoryRequirementsView(memReqs.m_memReqs.size, memReqs.m_memReqs.alignment, &memReqs, VulkanMemoryRequirementsFuncs::HeapKeyFilter);
	}
} } }
