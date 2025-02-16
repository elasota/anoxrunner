#pragma once

#include "IncludeVulkan.h"

#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/SwapChain.h"
#include "rkit/Render/SwapChainFrame.h"

#include <cstdint>

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct IDisplay;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;
	class VulkanQueueProxyBase;

	struct IVulkanSurface
	{
		virtual ~IVulkanSurface() {}

		virtual VkSurfaceKHR GetSurface() const = 0;
	};

	class VulkanSwapChainSyncPointBase : public ISwapChainSyncPoint
	{
	public:
		virtual VkSemaphore GetAcquireSema() const = 0;
		virtual VkSemaphore GetPresentSema() const = 0;

		static Result Create(UniquePtr<VulkanSwapChainSyncPointBase> &outSyncPoint, VulkanDeviceBase &device);
	};

	class VulkanSwapChainPrototypeBase : public ISwapChainPrototype
	{
	public:
		static Result Create(UniquePtr<VulkanSwapChainPrototypeBase> &outSwapChainPrototype, VulkanDeviceBase &device, IDisplay &display);
	};

	class VulkanSwapChainBase : public ISwapChain
	{
	public:
		static Result Create(UniquePtr<VulkanSwapChainBase> &outSwapChain, VulkanDeviceBase &device, VulkanSwapChainPrototypeBase &prototype, uint8_t numImages,
			render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, VulkanQueueProxyBase &queue);
	};
}
