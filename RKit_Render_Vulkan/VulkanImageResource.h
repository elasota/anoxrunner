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

	class VulkanImagePrototype final : public IImagePrototype
	{
	public:
		explicit VulkanImagePrototype(VulkanDeviceBase &device, VkImage image);
		~VulkanImagePrototype();

		MemoryRequirementsView GetMemoryRequirements() const override;

		static Result Create(UniquePtr<VulkanImagePrototype> &outImagePrototype, VulkanDeviceBase &device,
			const ImageSpec &imageSpec, const ImageResourceSpec &resourceSpec, const ConstSpan<IBaseCommandQueue *> &restrictedQueues);

	private:
		VulkanDeviceBase &m_device;
		VkImage m_image;
		VulkanDeviceMemoryRequirements m_memRequirements;
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
} } } // rkit::render::vulkan
