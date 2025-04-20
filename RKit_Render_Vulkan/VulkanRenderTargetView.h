#pragma once

#include "rkit/Render/RenderTargetView.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/ImagePlane.h"

#include "IncludeVulkan.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanRenderTargetViewBase : public IRenderTargetView
	{
	public:
		virtual VkImageView GetImageView() const = 0;
		virtual VkImageAspectFlags GetImageAspectFlags() const = 0;

		static Result Create(UniquePtr<VulkanRenderTargetViewBase> &outRTV, VulkanDeviceBase &device, VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageViewType imageViewType, uint32_t mipSlice, ImagePlane plane, uint32_t firstArrayElement, uint32_t arraySize);
	};
}
