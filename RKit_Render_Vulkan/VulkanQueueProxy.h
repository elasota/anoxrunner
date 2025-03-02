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

namespace rkit::render::vulkan
{
	struct VulkanDeviceAPI;
	class VulkanDeviceBase;

	class VulkanQueueProxyBase : public IGraphicsComputeCommandQueue, public IInternalCommandQueue
	{
	public:
		virtual Result Flush() = 0;
		virtual uint32_t GetQueueFamily() const = 0;

		virtual Result AddBinarySemaSignal(VkSemaphore sema) = 0;
		virtual Result AddBinarySemaWait(VkSemaphore sema, VkPipelineStageFlags stageFlags) = 0;
		virtual Result AddCommandList(VkCommandBuffer commandList) = 0;

		virtual VkQueue GetVkQueue() const = 0;

		static Result Create(UniquePtr<VulkanQueueProxyBase> &outQueueProxy, IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI);
	};
}
