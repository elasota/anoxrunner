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

namespace rkit
{
	template<class TSrc, class TDest>
	struct DynamicDowncaster;
}

namespace rkit::render::vulkan
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
} // rkit::render::vulkan

namespace rkit
{
	template<>
	struct DynamicDowncaster<render::vulkan::VulkanQueueProxyBase, render::ICopyCommandQueue>
	{
		static render::ICopyCommandQueue *Cast(render::vulkan::VulkanQueueProxyBase *src);
	};
}

namespace rkit
{
	inline render::ICopyCommandQueue *DynamicDowncaster<render::vulkan::VulkanQueueProxyBase, render::ICopyCommandQueue>::Cast(render::vulkan::VulkanQueueProxyBase *src)
	{
		return DynamicDowncaster<render::IGraphicsComputeCommandQueue, render::IComputeCommandQueue>::Cast(src);
	}
}

