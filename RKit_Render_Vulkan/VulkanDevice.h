#pragma once

#include "rkit/Render/RenderDevice.h"

#include "rkit/Core/StringProto.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct IMutex;

	template<class T>
	class UniquePtr;

	template<class T>
	class Optional;

	template<class T>
	class RCPtr;

	template<class T>
	class Vector;
}

namespace rkit { namespace render
{
	class RenderDeviceCaps;
	class RenderDeviceRequirements;
	struct HeapSpec;
} } // rkit::render

namespace rkit { namespace render { namespace vulkan
{
	struct VulkanDeviceAPI;
	struct VulkanGlobalAPI;
	struct VulkanInstanceAPI;

	struct VulkanGlobalPlatformAPI;
	struct VulkanInstancePlatformAPI;
	struct VulkanDevicePlatformAPI;

	class RenderVulkanPhysicalDevice;
	class VulkanQueueProxyBase;

	template<class T>
	struct IResourcePool;

	class VulkanDeviceBase : public IRenderDevice
	{
	public:
		struct QueueFamilySpec
		{
			uint32_t m_queueFamily = 0;
			uint32_t m_numQueues = 0;
		};

		virtual VkDevice GetDevice() const = 0;
		virtual VkInstance GetInstance() const = 0;
		virtual const VkAllocationCallbacks *GetAllocCallbacks() const = 0;

		virtual const VulkanGlobalAPI &GetGlobalAPI() const = 0;
		virtual const VulkanInstanceAPI &GetInstanceAPI() const = 0;
		virtual const VulkanDeviceAPI &GetDeviceAPI() const = 0;

		virtual const VulkanGlobalPlatformAPI &GetGlobalPlatformAPI() const = 0;
		virtual const VulkanInstancePlatformAPI &GetInstancePlatformAPI() const = 0;
		virtual const VulkanDevicePlatformAPI &GetDevicePlatformAPI() const = 0;

		virtual const RenderVulkanPhysicalDevice &GetPhysDevice() const = 0;

		virtual IResourcePool<VkSemaphore> &GetBinarySemaPool() const = 0;
		virtual IResourcePool<VkFence> &GetFencePool() const = 0;

		virtual VulkanQueueProxyBase &GetQueueByID(size_t queueID) = 0;
		virtual size_t GetQueueCount() const = 0;

		virtual bool MemoryTypeMeetsHeapSpec(uint32_t memoryType, const HeapSpec &heapSpec) const = 0;
		virtual bool MemoryTypeIsSubset(uint32_t candidateType, uint32_t largerType) const = 0;

		static Result CreateDevice(UniquePtr<IRenderDevice> &outDevice,
			const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki,
			const VulkanGlobalPlatformAPI &vkg_p, const VulkanInstancePlatformAPI &vki_p,
			VkInstance inst, VkDevice device,
			const QueueFamilySpec (&queues)[static_cast<size_t>(CommandQueueType::kCount)], const VkAllocationCallbacks *allocCallbacks,
			const RenderDeviceCaps &caps, const RenderDeviceRequirements &reqs,
			const RCPtr<RenderVulkanPhysicalDevice> &physDevice, Vector<AsciiStringView> &&enabledExts,
			const VkPhysicalDeviceMemoryProperties &memProperties);
	};
} } } // rkit::render::vulkan
