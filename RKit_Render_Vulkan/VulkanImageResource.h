#pragma once

#include "rkit/Render/ImageResource.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	class VulkanImageContainer : public IImageResource
	{
	public:
		VkImage GetVkImage() const;
		VkImageAspectFlags GetAllAspectFlags() const;

	protected:
		VkImage m_image = VK_NULL_HANDLE;
		VkImageAspectFlags m_allAspectFlags = 0;
	};
}

namespace rkit::render::vulkan
{
	inline VkImage VulkanImageContainer::GetVkImage() const
	{
		return m_image;
	}

	inline VkImageAspectFlags VulkanImageContainer::GetAllAspectFlags() const
	{
		return m_allAspectFlags;
	}
}
