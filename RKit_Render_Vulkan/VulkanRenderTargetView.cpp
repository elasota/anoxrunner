#include "VulkanRenderTargetView.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"

namespace rkit { namespace render { namespace vulkan
{
	class VulkanRenderTargetView final : public VulkanRenderTargetViewBase
	{
	public:
		explicit VulkanRenderTargetView(VulkanDeviceBase &device);
		~VulkanRenderTargetView();

		VkImageView GetImageView() const override;
		VkImageAspectFlags GetImageAspectFlags() const override;

		Result Initialize(VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageViewType imageViewType, uint32_t mipSlice, ImagePlane plane, uint32_t firstArrayElement, uint32_t arraySize);

	private:
		VulkanDeviceBase &m_device;
		VkImageView m_imageView = VK_NULL_HANDLE;
		VkImageAspectFlags m_imageAspectFlags = 0;
	};

	VulkanRenderTargetView::VulkanRenderTargetView(VulkanDeviceBase &device)
		: m_device(device)
	{
	}

	VulkanRenderTargetView::~VulkanRenderTargetView()
	{
		if (m_imageView != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyImageView(m_device.GetDevice(), m_imageView, m_device.GetAllocCallbacks());
	}

	VkImageView VulkanRenderTargetView::GetImageView() const
	{
		return m_imageView;
	}

	VkImageAspectFlags VulkanRenderTargetView::GetImageAspectFlags() const
	{
		return m_imageAspectFlags;
	}


	Result VulkanRenderTargetView::Initialize(VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageViewType imageViewType, uint32_t mipSlice, ImagePlane plane, uint32_t firstArrayElement, uint32_t arraySize)
	{
		m_imageAspectFlags = imageAspectFlags;

		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = imageViewType;
		createInfo.format = format;

		switch (plane)
		{
		case ImagePlane::kColor:
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		case ImagePlane::kDepth:
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
		case ImagePlane::kStencil:
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
		default:
			return ResultCode::kInternalError;
		}

		createInfo.subresourceRange.baseMipLevel = mipSlice;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = firstArrayElement;
		createInfo.subresourceRange.layerCount = arraySize;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateImageView(m_device.GetDevice(), &createInfo, m_device.GetAllocCallbacks(), &m_imageView));

		return ResultCode::kOK;
	}

	Result VulkanRenderTargetViewBase::Create(UniquePtr<VulkanRenderTargetViewBase> &outRTV, VulkanDeviceBase &device, VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageViewType imageViewType, uint32_t mipSlice, ImagePlane plane, uint32_t firstArrayElement, uint32_t arraySize)
	{
		UniquePtr<VulkanRenderTargetView> rtv;
		RKIT_CHECK(New<VulkanRenderTargetView>(rtv, device));

		RKIT_CHECK(rtv->Initialize(image, format, imageAspectFlags, imageViewType, mipSlice, plane, firstArrayElement, arraySize));

		outRTV = std::move(rtv);

		return ResultCode::kOK;
	}
} } } // rkit::render::vulkan
