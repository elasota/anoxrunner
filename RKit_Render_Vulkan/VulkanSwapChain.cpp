#include "VulkanSwapChain.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanPhysDevice.h"
#include "VulkanPlatformSpecific.h"
#include "VulkanQueueProxy.h"
#include "VulkanResourcePool.h"
#include "VulkanUtils.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/SwapChainFrame.h"

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Result.h"

namespace rkit::render::vulkan
{
	class VulkanSwapChainSubframe final : public ISwapChainSubframe
	{
	public:
		VulkanSwapChainSubframe();

		Result Initialize(uint32_t imageIndex, uint32_t imageLayer);

	private:
		uint32_t m_imageIndex = 0;
		uint32_t m_imageLayer = 0;
	};

	class VulkanSwapChainFrame final : public ISwapChainFrame
	{
	public:
		VulkanSwapChainFrame();

		Result Initialize(const Span<VulkanSwapChainSubframe> &subframes, VkImage image);

		UniqueResourceRef<VkSemaphore> *GetDoneSemaRefPtr();
		UniqueResourceRef<VkSemaphore> *GetStartedSemaRefPtr();

	private:
		ISwapChainSubframe *GetSubframe(size_t index) const override;

		UniqueResourceRef<VkSemaphore> m_doneSema;
		UniqueResourceRef<VkSemaphore> m_startedSema;

		Span<VulkanSwapChainSubframe> m_subframes;
		VkImage m_image = VK_NULL_HANDLE;
	};

	class VulkanSwapChain final : public VulkanSwapChainBase
	{
	public:
		VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers, SwapChainWriteBehavior writeBehavior, QueueProxy &queue);
		~VulkanSwapChain();

		Result Initialize(render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, uint32_t queueFamily);

		void GetExtents(uint32_t &outWidth, uint32_t &outHeight) const override;

		Result AcquireFrame(ISwapChainFrame *&outFrame) override;
		Result Present() override;

	private:
		VulkanDeviceBase &m_device;
		IDisplay &m_display;

		UniquePtr<IVulkanSurface> m_surface;
		VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;

		uint32_t m_width = 0;
		uint32_t m_height = 0;

		size_t m_numBuffers = 0;
		size_t m_nextSyncIndex = 0;

		Vector<VkImage> m_images;
		Vector<VulkanSwapChainFrame> m_swapChainFrames;
		Vector<VulkanSwapChainSubframe> m_swapChainSubframes;

		SwapChainWriteBehavior m_writeBehavior;
		QueueProxy &m_queue;

		Optional<uint32_t> m_acquiredImage;
	};

	VulkanSwapChainSubframe::VulkanSwapChainSubframe()
	{
	}

	Result VulkanSwapChainSubframe::Initialize(uint32_t imageIndex, uint32_t imageLayer)
	{
		m_imageIndex = imageIndex;
		m_imageLayer = imageLayer;

		return ResultCode::kOK;
	}

	VulkanSwapChainFrame::VulkanSwapChainFrame()
	{
	}

	Result VulkanSwapChainFrame::Initialize(const Span<VulkanSwapChainSubframe> &subframes, VkImage image)
	{
		m_subframes = subframes;

		return ResultCode::kOK;
	}

	UniqueResourceRef<VkSemaphore> *VulkanSwapChainFrame::GetDoneSemaRefPtr()
	{
		return &m_doneSema;
	}

	UniqueResourceRef<VkSemaphore> *VulkanSwapChainFrame::GetStartedSemaRefPtr()
	{
		return &m_startedSema;
	}

	ISwapChainSubframe *VulkanSwapChainFrame::GetSubframe(size_t index) const
	{
		return &m_subframes[index];
	}

	VulkanSwapChain::VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers, SwapChainWriteBehavior writeBehavior, QueueProxy &queue)
		: m_device(device)
		, m_display(display)
		, m_numBuffers(static_cast<size_t>(numBackBuffers) + 1)
		, m_writeBehavior(writeBehavior)
		, m_queue(queue)
	{
	}

	VulkanSwapChain::~VulkanSwapChain()
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		// This is broken and needs VK_EXT_swapchain_maintenance1 to properly fix
		// https://github.com/KhronosGroup/Vulkan-Docs/issues/1678
		vkd.vkDeviceWaitIdle(m_device.GetDevice());

		if (m_swapChain != VK_NULL_HANDLE)
			vkd.vkDestroySwapchainKHR(m_device.GetDevice(), m_swapChain, m_device.GetAllocCallbacks());

		m_surface.Reset();
	}

	Result VulkanSwapChain::Initialize(render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, uint32_t queueFamily)
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();
		const VulkanInstanceAPI &vki = m_device.GetInstanceAPI();

		RKIT_CHECK(platform::CreateSurfaceFromDisplay(m_surface, m_device, m_display));

		VkSurfaceCapabilitiesKHR surfaceCaps = {};
		RKIT_VK_CHECK(vki.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_device.GetPhysDevice().GetPhysDevice(), m_surface->GetSurface(), &surfaceCaps));

		uint32_t numImages = static_cast<uint32_t>(m_numBuffers);
		if (numImages < surfaceCaps.minImageCount)
			numImages = surfaceCaps.minImageCount;
		else if (surfaceCaps.maxImageCount != 0 && numImages > surfaceCaps.maxImageCount)
			return ResultCode::kOperationFailed;

		if (m_display.GetSimultaneousImageCount() > surfaceCaps.maxImageArrayLayers)
			return ResultCode::kInternalError;

		const uint32_t simultaneousImageCount = m_display.GetSimultaneousImageCount();

		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

		swapchainCreateInfo.surface = m_surface->GetSurface();
		swapchainCreateInfo.minImageCount = numImages;
		RKIT_CHECK(VulkanUtils::ResolveRenderTargetFormat(swapchainCreateInfo.imageFormat, fmt));
		swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		swapchainCreateInfo.imageExtent = surfaceCaps.currentExtent;
		swapchainCreateInfo.imageArrayLayers = simultaneousImageCount;

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

		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.preTransform = surfaceCaps.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		swapchainCreateInfo.clipped = VK_TRUE;

		RKIT_VK_CHECK(vkd.vkCreateSwapchainKHR(m_device.GetDevice(), &swapchainCreateInfo, m_device.GetAllocCallbacks(), &m_swapChain));

		m_width = surfaceCaps.currentExtent.width;
		m_height = surfaceCaps.currentExtent.height;

		// Get the swapchain images
		uint32_t imageCount = 0;
		RKIT_VK_CHECK(vkd.vkGetSwapchainImagesKHR(m_device.GetDevice(), m_swapChain, &imageCount, nullptr));

		RKIT_CHECK(m_images.Resize(imageCount));
		for (VkImage &img : m_images)
			img = VK_NULL_HANDLE;

		RKIT_CHECK(m_swapChainFrames.Resize(imageCount));
		RKIT_CHECK(m_swapChainSubframes.Resize(imageCount * simultaneousImageCount));

		for (uint32_t fi = 0; fi < imageCount; fi++)
		{
			for (uint32_t sfi = 0; sfi < simultaneousImageCount; sfi++)
			{
				RKIT_CHECK(m_swapChainSubframes[fi * simultaneousImageCount + sfi].Initialize(fi, sfi));
			}
		}

		RKIT_VK_CHECK(vkd.vkGetSwapchainImagesKHR(m_device.GetDevice(), m_swapChain, &imageCount, m_images.GetBuffer()));

		for (uint32_t i = 0; i < imageCount; i++)
		{
			RKIT_CHECK(m_swapChainFrames[i].Initialize(m_swapChainSubframes.ToSpan().SubSpan(i * simultaneousImageCount, simultaneousImageCount), m_images[i]));
		}

		return ResultCode::kOK;
	}

	void VulkanSwapChain::GetExtents(uint32_t &outWidth, uint32_t &outHeight) const
	{
		outWidth = m_width;
		outHeight = m_height;
	}

	Result VulkanSwapChain::AcquireFrame(ISwapChainFrame *&outFrame)
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();

		if (m_acquiredImage.IsSet())
			return ResultCode::kInternalError;

		UniqueResourceRef<VkSemaphore> acquireSema;
		RKIT_CHECK(acquireSema.AcquireFrom(m_device.GetBinarySemaPool()));

		uint32_t imgIndex = 0;
		RKIT_VK_CHECK(vkd.vkAcquireNextImageKHR(m_device.GetDevice(), m_swapChain, UINT64_MAX, acquireSema.GetResource(), VK_NULL_HANDLE, &imgIndex));

		VkPipelineStageFlagBits stageFlags = static_cast<VkPipelineStageFlagBits>(0);
		switch (m_writeBehavior)
		{
		case SwapChainWriteBehavior::RenderTarget:
			stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		default:
			return ResultCode::kNotYetImplemented;
		}

		RKIT_CHECK(m_queue.AddBinarySemaWait(acquireSema.GetResource(), stageFlags));

		*m_swapChainFrames[imgIndex].GetStartedSemaRefPtr() = std::move(acquireSema);
		m_swapChainFrames[imgIndex].GetDoneSemaRefPtr()->Reset();

		m_acquiredImage = imgIndex;

		outFrame = &m_swapChainFrames[imgIndex];

		return ResultCode::kOK;
	}

	Result VulkanSwapChain::Present()
	{
		if (!m_acquiredImage.IsSet())
			return ResultCode::kInternalError;

		uint32_t imageIndex = m_acquiredImage.Get();

		UniqueResourceRef<VkSemaphore> *finishedSema = m_swapChainFrames[imageIndex].GetDoneSemaRefPtr();
		if (!finishedSema->IsValid())
		{
			RKIT_CHECK(finishedSema->AcquireFrom(m_device.GetBinarySemaPool()));
		}

		VkPipelineStageFlagBits stageFlags = static_cast<VkPipelineStageFlagBits>(0);
		switch (m_writeBehavior)
		{
		case SwapChainWriteBehavior::RenderTarget:
			stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		default:
			return ResultCode::kNotYetImplemented;
		}

		RKIT_CHECK(m_queue.AddBinarySemaSignal(finishedSema->GetResource(), stageFlags));
		RKIT_CHECK(m_queue.Flush());

		VkSemaphore vkFinishedSema = finishedSema->GetResource();

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &vkFinishedSema;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapChain;
		presentInfo.pImageIndices = &imageIndex;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkQueuePresentKHR(m_queue.GetVkQueue(), &presentInfo));

		m_acquiredImage.Reset();

		return ResultCode::kOK;
	}

	Result VulkanSwapChainBase::Create(UniquePtr<VulkanSwapChainBase> &outSwapChain, VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, QueueProxy &queue)
	{
		uint32_t queueFamily = queue.GetQueueFamily();

		UniquePtr<VulkanSwapChain> swapChain;
		RKIT_CHECK(New<VulkanSwapChain>(swapChain, device, display, numBackBuffers, writeBehavior, queue));

		RKIT_CHECK(swapChain->Initialize(fmt, writeBehavior, queueFamily));

		outSwapChain = std::move(swapChain);

		return ResultCode::kOK;
	}
}
