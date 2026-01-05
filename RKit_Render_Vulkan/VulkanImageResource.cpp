#include "VulkanImageResource.h"
#include "VulkanMemoryRequirements.h"

#include "VulkanAPI.h"
#include "VulkanQueueProxy.h"
#include "VulkanDevice.h"
#include "VulkanCheck.h"
#include "VulkanUtils.h"

#include "rkit/Render/CommandQueue.h"
#include "rkit/Render/ImageSpec.h"
#include "rkit/Render/Memory.h"

#include "rkit/Core/Span.h"
#include "rkit/Core/Vector.h"

namespace rkit { namespace render { namespace vulkan {

	VulkanImage::VulkanImage(VulkanDeviceBase &device, VkImage image, VkImageAspectFlags allAspectFlags)
		: m_device(device)
	{
		m_allAspectFlags = allAspectFlags;
		m_image = image;
	}

	VulkanImage::~VulkanImage()
	{
		if (m_image != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyImage(m_device.GetDevice(), m_image, m_device.GetAllocCallbacks());
	}

	VulkanImagePrototype::VulkanImagePrototype(VulkanDeviceBase &device, VkImage image, VkImageAspectFlags allAspects)
		: m_device(device)
		, m_image(image)
		, m_memRequirements { &device }
		, m_allAspects(allAspects)
	{
		m_device.GetDeviceAPI().vkGetImageMemoryRequirements(m_device.GetDevice(), image, &m_memRequirements.m_memReqs);
	}

	VulkanImagePrototype::~VulkanImagePrototype()
	{
		if (m_image != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyImage(m_device.GetDevice(), m_image, m_device.GetAllocCallbacks());
	}

	MemoryRequirementsView VulkanImagePrototype::GetMemoryRequirements() const
	{
		return ExportMemoryRequirements(m_memRequirements);
	}

	Result VulkanImagePrototype::Create(UniquePtr<VulkanImagePrototype> &outImagePrototype, VulkanDeviceBase &device,
		const ImageSpec &imageSpec, const ImageResourceSpec &resourceSpec, const ConstSpan<IBaseCommandQueue *> &concurrentQueues)
	{
		VkImageCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

		if (imageSpec.m_cubeMap)
			createInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		if (imageSpec.m_arrayLayers > 1)
			createInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;

		createInfo.imageType = VK_IMAGE_TYPE_2D;
		if (imageSpec.m_depth > 1)
			createInfo.imageType = VK_IMAGE_TYPE_3D;

		switch (imageSpec.m_format)
		{
		case TextureFormat::BGRA_UNorm8:
			createInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
			break;
		case TextureFormat::BGRA_UNorm8_sRGB:
			createInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
			break;
		case TextureFormat::RGBA_UNorm8:
			createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case TextureFormat::RGBA_UNorm8_sRGB:
			createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		case TextureFormat::RG_UNorm8:
			createInfo.format = VK_FORMAT_R8G8_UNORM;
			break;
		case TextureFormat::RG_UNorm8_sRGB:
			createInfo.format = VK_FORMAT_R8G8_SRGB;
			break;
		case TextureFormat::R_UNorm8:
			createInfo.format = VK_FORMAT_R8G8_UNORM;
			break;
		case TextureFormat::R_UNorm8_sRGB:
			createInfo.format = VK_FORMAT_R8_SRGB;
			break;
		default:
			return ResultCode::kInvalidParameter;
		}

		createInfo.extent.width = imageSpec.m_width;
		createInfo.extent.height = imageSpec.m_height;
		createInfo.extent.depth = imageSpec.m_depth;
		createInfo.mipLevels = imageSpec.m_mipLevels;
		createInfo.arrayLayers = imageSpec.m_arrayLayers;
		createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		createInfo.tiling = resourceSpec.m_linearLayout ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

		for (TextureUsageFlag usageFlag : resourceSpec.m_usage)
		{
			switch (usageFlag)
			{
			case TextureUsageFlag::kCopySrc:
				createInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				break;
			case TextureUsageFlag::kCopyDest:
				createInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				break;
			case TextureUsageFlag::kSampled:
				createInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
				break;
			default:
				return ResultCode::kInvalidParameter;
			}
		}

		Vector<uint32_t> restrictedQueueFamilies;

		if (concurrentQueues.Count() == 0)
			createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		else
		{
			createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

			for (IBaseCommandQueue *queue : concurrentQueues)
			{
				VulkanQueueProxyBase *internalQueue = static_cast<VulkanQueueProxyBase *>(queue->ToInternalCommandQueue());

				const uint32_t queueFamily = internalQueue->GetQueueFamily();

				bool isNewQueue = true;
				for (uint32_t existingQueueFamily : restrictedQueueFamilies)
				{
					if (existingQueueFamily == queueFamily)
					{
						isNewQueue = false;
						break;
					}
				}

				if (isNewQueue)
				{
					RKIT_CHECK(restrictedQueueFamilies.Append(queueFamily));
				}
			}

			createInfo.pQueueFamilyIndices = restrictedQueueFamilies.GetBuffer();
			createInfo.queueFamilyIndexCount = static_cast<uint32_t>(restrictedQueueFamilies.Count());
		}

		createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkImage image = VK_NULL_HANDLE;
		RKIT_VK_CHECK(device.GetDeviceAPI().vkCreateImage(device.GetDevice(), &createInfo, device.GetAllocCallbacks(), &image));

		VkImageAspectFlags allAspects = VK_IMAGE_ASPECT_COLOR_BIT;

		Result createResult = rkit::New<VulkanImagePrototype>(outImagePrototype, device, image, allAspects);

		if (!utils::ResultIsOK(createResult))
			device.GetDeviceAPI().vkDestroyImage(device.GetDevice(), image, device.GetAllocCallbacks());

		return createResult;
	}

} } }
