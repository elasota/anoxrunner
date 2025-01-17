#pragma once

#include "rkit/Render/CommandQueueType.h"
#include "rkit/Render/CommandAllocator.h"

#include <cstdint>

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanCommandAllocatorBase : public IGraphicsComputeCommandAllocator
	{
	public:
		static Result Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, CommandQueueType queueType, uint32_t queueFamily);
	};
}
