#pragma once

#include "rkit/Render/RenderTargetView.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/TexturePlane.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;

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

		static Result Create(UniquePtr<VulkanRenderTargetViewBase> &outRTV, VulkanDeviceBase &device, VkImage image, VkFormat format, VkImageViewType imageViewType, uint32_t mipSlice, TexturePlane plane, uint32_t firstArrayElement, uint32_t arraySize);
	};
}
