#pragma once

#include "rkit/Render/DepthStencilView.h"

#include "IncludeVulkan.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanDepthStencilViewBase : public IDepthStencilView
	{
	public:
		virtual VkImageView GetImageView() const = 0;
		virtual VkImageAspectFlags GetAspectFlags() const = 0;

		static Result Create(UniquePtr<VulkanDepthStencilViewBase> &outRTV, VulkanDeviceBase &device);

	private:
	};
}
