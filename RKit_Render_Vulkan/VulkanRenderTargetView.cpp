#pragma once

#include "VulkanRenderTargetView.h"

namespace rkit::render::vulkan
{
	class VulkanRenderTargetView final : public VulkanRenderTargetViewBase
	{
		VkImageView GetImageView() const override;
	};
}
