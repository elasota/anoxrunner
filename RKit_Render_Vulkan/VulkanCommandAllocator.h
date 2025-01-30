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
		virtual CommandQueueType GetQueueType() const = 0;
		virtual bool IsBundle() const = 0;

		static Result Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, CommandQueueType queueType, bool isBundle, uint32_t queueFamily);
	};
}
