#pragma once

#include "IncludeVulkan.h"

#include "rkit/Render/SwapChain.h"

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

	struct IVulkanSurface
	{
		virtual ~IVulkanSurface() {}

		virtual VkSurfaceKHR GetSurface() const = 0;
	};

	class VulkanSwapChainBase : public ISwapChain
	{
	public:
		static Result Create(UniquePtr<VulkanSwapChainBase> &outSwapChain, VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers);
	};
}
