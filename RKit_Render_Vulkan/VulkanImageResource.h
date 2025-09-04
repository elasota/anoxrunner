#pragma once

#include "rkit/Render/ImageResource.h"

#include "rkit/Core/SpanProtos.h"

#include "IncludeVulkan.h"
#include "VulkanMemoryRequirements.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit { namespace render {
	struct IBaseCommandQueue;
	struct ImageSpec;
	struct ImageResourceSpec;
} }

namespace rkit { namespace render { namespace vulkan
{
	class VulkanDeviceBase;

	class VulkanImageContainer : public IImageResource
	{
	public:
		VkImage GetVkImage() const;
		VkImageAspectFlags GetAllAspectFlags() const;

	protected:
		VkImage m_image = VK_NULL_HANDLE;
		VkImageAspectFlags m_allAspectFlags = 0;
	};

	class VulkanImage : public VulkanImageContainer
	{
	public:
		VulkanImage(VulkanDeviceBase &device, VkImage image, VkImageAspectFlags allAspectFlags);
		~VulkanImage();

	private:
		VulkanDeviceBase &m_device;
	};

	class VulkanImagePrototype final : public IImagePrototype
	{
	public:
		explicit VulkanImagePrototype(VulkanDeviceBase &device, VkImage image, VkImageAspectFlags allAspects);
		~VulkanImagePrototype();

		MemoryRequirementsView GetMemoryRequirements() const override;

		VkImageAspectFlags GetAllAspectFlags() const;
		VkImage GetImage() const;

		void DetachImage();

		static Result Create(UniquePtr<VulkanImagePrototype> &outImagePrototype, VulkanDeviceBase &device,
			const ImageSpec &imageSpec, const ImageResourceSpec &resourceSpec, const ConstSpan<IBaseCommandQueue *> &concurrentQueues);

	private:
		VulkanDeviceBase &m_device;
		VkImage m_image;
		VulkanDeviceMemoryRequirements m_memRequirements;
		VkImageAspectFlags m_allAspects;
	};
} } } // rkit::render::vulkan

namespace rkit { namespace render { namespace vulkan
{
	inline VkImage VulkanImageContainer::GetVkImage() const
	{
		return m_image;
	}

	inline VkImageAspectFlags VulkanImageContainer::GetAllAspectFlags() const
	{
		return m_allAspectFlags;
	}

	inline VkImageAspectFlags VulkanImagePrototype::GetAllAspectFlags() const
	{
		return m_allAspects;
	}

	inline VkImage VulkanImagePrototype::GetImage() const
	{
		return m_image;
	}

	inline void VulkanImagePrototype::DetachImage()
	{
		m_image = VK_NULL_HANDLE;
	}
} } } // rkit::render::vulkan
