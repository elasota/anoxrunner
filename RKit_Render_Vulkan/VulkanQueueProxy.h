#pragma once

#include "rkit/Render/CommandQueue.h"

#include "rkit/Core/ResizableRingBuffer.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Vector.h"

#include "VulkanResourcePool.h"
#include "VulkanQueueMask.h"
#include "VulkanSync.h"

#include "IncludeVulkan.h"

namespace rkit { namespace render { namespace vulkan
{
	struct VulkanDeviceAPI;
	class VulkanDeviceBase;

	class VulkanQueueProxyBase : public IGraphicsComputeCommandQueue, public IInternalCommandQueue
	{
	public:
		virtual uint32_t GetQueueFamily() const = 0;

		virtual VkQueue GetVkQueue() const = 0;

		static Result Create(UniquePtr<VulkanQueueProxyBase> &outQueueProxy, IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI);
	};
} } } // rkit::render::vulkan
