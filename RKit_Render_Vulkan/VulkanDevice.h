#pragma once

#include "rkit/Render/RenderDevice.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;

	template<class T>
	class Optional;
}

namespace rkit::render
{
	class RenderDeviceCaps;
}

namespace rkit::render::vulkan
{
	struct VulkanDeviceAPI;
	struct VulkanGlobalAPI;
	struct VulkanInstanceAPI;

	struct VulkanDeviceBase : public IRenderDevice
	{
		struct QueueFamilySpec
		{
			uint32_t m_queueFamily = 0;
			uint32_t m_numQueues = 0;
		};

		virtual VkDevice GetDevice() const = 0;
		virtual const VkAllocationCallbacks *GetAllocCallbacks() const = 0;

		virtual const VulkanGlobalAPI &GetGlobalAPI() const = 0;
		virtual const VulkanInstanceAPI &GetInstanceAPI() const = 0;
		virtual const VulkanDeviceAPI &GetDeviceAPI() const = 0;

		static Result CreateDevice(UniquePtr<IRenderDevice> &outDevice, const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, VkInstance inst, VkDevice device, const QueueFamilySpec (&queues)[static_cast<size_t>(CommandQueueType::kCount)], const VkAllocationCallbacks *allocCallbacks, const RenderDeviceCaps &caps);
	};
}
