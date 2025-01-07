#include "VulkanSwapChain.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanPhysDevice.h"
#include "VulkanPlatformSpecific.h"
#include "VulkanUtils.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDefs.h"

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Result.h"

namespace rkit::render::vulkan
{
	class VulkanSwapChain final : public VulkanSwapChainBase
	{
	public:
		VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers);
		~VulkanSwapChain();

		Result Initialize(render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, const Span<uint32_t> &queueFamilies);

		void GetExtents(uint32_t &outWidth, uint32_t &outHeight) const override;

	private:
		VulkanDeviceBase &m_device;
		IDisplay &m_display;
		uint8_t m_numBackBuffers = 0;

		UniquePtr<IVulkanSurface> m_surface;
		VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
	};

	VulkanSwapChain::VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers)
		: m_device(device)
		, m_display(display)
		, m_numBackBuffers(numBackBuffers)
	{
	}

	VulkanSwapChain::~VulkanSwapChain()
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();
		if (m_swapChain != VK_NULL_HANDLE)
			vkd.vkDestroySwapchainKHR(m_device.GetDevice(), m_swapChain, m_device.GetAllocCallbacks());

		m_surface.Reset();
	}

	Result VulkanSwapChain::Initialize(render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, const Span<uint32_t> &queueFamilies)
	{
		RKIT_CHECK(platform::CreateSurfaceFromDisplay(m_surface, m_device, m_display));

		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();
		const VulkanInstanceAPI &vki = m_device.GetInstanceAPI();

		VkSurfaceCapabilitiesKHR surfaceCaps = {};
		RKIT_VK_CHECK(vki.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.GetPhysDevice().GetPhysDevice(), m_surface->GetSurface(), &surfaceCaps));

		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

		swapchainCreateInfo.surface = m_surface->GetSurface();
		swapchainCreateInfo.minImageCount = m_numBackBuffers + 1;
		RKIT_CHECK(VulkanUtils::ResolveRenderTargetFormat(swapchainCreateInfo.imageFormat, fmt));
		swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		swapchainCreateInfo.imageExtent = surfaceCaps.currentExtent;
		swapchainCreateInfo.imageArrayLayers = 1;

		switch (writeBehavior)
		{
		case SwapChainWriteBehavior::RenderTarget:
			swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case SwapChainWriteBehavior::Copy:
			swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			break;
		default:
			return ResultCode::kInternalError;
		}

		if (queueFamilies.Count() == 1)
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		else
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.Count());
			swapchainCreateInfo.pQueueFamilyIndices = queueFamilies.Ptr();
		}

		swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;

		RKIT_VK_CHECK(vkd.vkCreateSwapchainKHR(m_device.GetDevice(), &swapchainCreateInfo, m_device.GetAllocCallbacks(), &m_swapChain));

		m_width = surfaceCaps.currentExtent.width;
		m_height = surfaceCaps.currentExtent.height;

		return ResultCode::kOK;
	}

	void VulkanSwapChain::GetExtents(uint32_t &outWidth, uint32_t &outHeight) const
	{
		outWidth = m_width;
		outHeight = m_height;
	}

	Result VulkanSwapChainBase::Create(UniquePtr<VulkanSwapChainBase> &outSwapChain, VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, const Span<uint32_t> &queueFamilies)
	{
		UniquePtr<VulkanSwapChain> swapChain;
		RKIT_CHECK(New<VulkanSwapChain>(swapChain, device, display, numBackBuffers));

		RKIT_CHECK(swapChain->Initialize(fmt, writeBehavior, queueFamilies));

		outSwapChain = std::move(swapChain);

		return ResultCode::kOK;
	}
}
