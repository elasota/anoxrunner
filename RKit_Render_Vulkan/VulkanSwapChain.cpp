#include "VulkanSwapChain.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanImageResource.h"
#include "VulkanPhysDevice.h"
#include "VulkanPlatformSpecific.h"
#include "VulkanRenderTargetView.h"
#include "VulkanQueueProxy.h"
#include "VulkanResourcePool.h"
#include "VulkanUtils.h"

#include "rkit/Render/DisplayManager.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/SwapChainFrame.h"

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Result.h"

namespace rkit::render::vulkan
{
	class VulkanSwapChainFrame final : public ISwapChainFrame
	{
	public:
		Result Initialize(size_t imageIndex);

	private:
		size_t m_imageIndex = 0;
	};

	class VulkanSwapChainSyncPoint final : public VulkanSwapChainSyncPointBase
	{
	public:
		explicit VulkanSwapChainSyncPoint(VulkanDeviceBase &device);
		~VulkanSwapChainSyncPoint();

		size_t GetFrameIndex() const override;
		VkSemaphore GetAcquireSema() const override;
		VkSemaphore GetPresentSema() const override;

		Result Initialize();

		Result AcquireFrame(VkSwapchainKHR swapChain, const Span<VkImage> &images, const Span<VulkanSwapChainFrame> &frames);
		Result Present(VulkanQueueProxyBase &queue, VkSwapchainKHR swapChain);

	private:
		VulkanDeviceBase &m_device;

		VkSemaphore m_acquireSema = VK_NULL_HANDLE;
		VkSemaphore m_presentSema = VK_NULL_HANDLE;

		uint32_t m_imageIndex = 0;
	};

	class VulkanSwapChainPrototype final : public VulkanSwapChainPrototypeBase
	{
	public:
		VulkanSwapChainPrototype(VulkanDeviceBase &device, IDisplay &display);

		Result Initialize();

		Result CheckQueueCompatibility(bool &outIsCompatible, const IBaseCommandQueue &commandQueue) const override;
		IDisplay &GetDisplay() const;

		UniquePtr<IVulkanSurface> TakeSurface();

	private:
		VulkanDeviceBase &m_device;
		IDisplay &m_display;

		UniquePtr<IVulkanSurface> m_surface;
	};

	class VulkanSwapChain final : public VulkanSwapChainBase
	{
	public:
		VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numImages, SwapChainWriteBehavior writeBehavior, VulkanQueueProxyBase &queue);
		~VulkanSwapChain();

		Result Initialize(VulkanSwapChainPrototype &prototype, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, uint32_t queueFamily);

		void GetExtents(uint32_t &outWidth, uint32_t &outHeight) const override;

		Result AcquireFrame(ISwapChainSyncPoint &syncPoint) override;
		Result Present(ISwapChainSyncPoint &syncPoint) override;

		IRenderTargetView *GetRenderTargetViewForFrame(size_t frameIndex) override;
		IImageResource *GetImageForFrame(size_t frameIndex) override;

	private:
		struct SwapChainImageContainer : public VulkanImageContainer
		{
			void SetImage(VkImage image, VkImageAspectFlags allAspectFlags);
		};

		VulkanDeviceBase &m_device;
		IDisplay &m_display;

		UniquePtr<IVulkanSurface> m_surface;
		VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;

		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_simultaneousImageCount = 0;

		size_t m_numBuffers = 0;
		size_t m_nextSyncIndex = 0;

		Vector<VkImage> m_images;
		Vector<UniquePtr<VulkanRenderTargetViewBase>> m_rtvs;
		Vector<SwapChainImageContainer> m_imageResources;
		Vector<VulkanSwapChainFrame> m_swapChainFrames;

		SwapChainWriteBehavior m_writeBehavior;
		VulkanQueueProxyBase &m_queue;

		Optional<uint32_t> m_acquiredImage;
	};

	Result VulkanSwapChainFrame::Initialize(size_t imageIndex)
	{
		m_imageIndex = imageIndex;

		return ResultCode::kOK;
	}

	VulkanSwapChainSyncPoint::VulkanSwapChainSyncPoint(VulkanDeviceBase &device)
		: m_device(device)
	{
	}

	VulkanSwapChainSyncPoint::~VulkanSwapChainSyncPoint()
	{
		if (m_acquireSema != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroySemaphore(m_device.GetDevice(), m_acquireSema, m_device.GetAllocCallbacks());
		if (m_presentSema != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroySemaphore(m_device.GetDevice(), m_presentSema, m_device.GetAllocCallbacks());
	}

	Result VulkanSwapChainSyncPoint::Initialize()
	{
		const VkDevice device = m_device.GetDevice();
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();
		const VkAllocationCallbacks *callbacks = m_device.GetAllocCallbacks();

		VkSemaphoreCreateInfo semaCreateInfo = {};
		semaCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		RKIT_VK_CHECK(vkd.vkCreateSemaphore(device, &semaCreateInfo, callbacks, &m_acquireSema));
		RKIT_VK_CHECK(vkd.vkCreateSemaphore(device, &semaCreateInfo, callbacks, &m_presentSema));

		return ResultCode::kOK;
	}

	size_t VulkanSwapChainSyncPoint::GetFrameIndex() const
	{
		return m_imageIndex;
	}

	VkSemaphore VulkanSwapChainSyncPoint::GetAcquireSema() const
	{
		return m_acquireSema;
	}

	VkSemaphore VulkanSwapChainSyncPoint::GetPresentSema() const
	{
		return m_presentSema;
	}

	Result VulkanSwapChainSyncPoint::AcquireFrame(VkSwapchainKHR swapChain, const Span<VkImage> &images, const Span<VulkanSwapChainFrame> &frames)
	{
		// TODO: Handle suboptimal
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkAcquireNextImageKHR(m_device.GetDevice(), swapChain, UINT64_MAX, m_acquireSema, VK_NULL_HANDLE, &m_imageIndex));

		return ResultCode::kOK;
	}

	Result VulkanSwapChainSyncPoint::Present(VulkanQueueProxyBase &queue, VkSwapchainKHR swapChain)
	{
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_presentSema;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &m_imageIndex;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkQueuePresentKHR(queue.GetVkQueue(), &presentInfo));

		return ResultCode::kOK;
	}

	VulkanSwapChainPrototype::VulkanSwapChainPrototype(VulkanDeviceBase &device, IDisplay &display)
		: m_device(device)
		, m_display(display)
	{
	}

	Result VulkanSwapChainPrototype::Initialize()
	{
		RKIT_CHECK(platform::CreateSurfaceFromDisplay(m_surface, m_device, m_display));

		return ResultCode::kOK;
	}

	Result VulkanSwapChainPrototype::CheckQueueCompatibility(bool &outIsCompatible, const IBaseCommandQueue &commandQueue) const
	{
		const VulkanQueueProxyBase *queueProxy = static_cast<const VulkanQueueProxyBase *>(commandQueue.ToInternalCommandQueue());

		VkBool32 supported = VK_FALSE;
		RKIT_VK_CHECK(m_device.GetInstanceAPI().vkGetPhysicalDeviceSurfaceSupportKHR(m_device.GetPhysDevice().GetPhysDevice(), queueProxy->GetQueueFamily(), m_surface->GetSurface(), &supported));

		outIsCompatible = (supported != VK_FALSE);

		return ResultCode::kOK;
	}

	IDisplay &VulkanSwapChainPrototype::GetDisplay() const
	{
		return m_display;
	}

	UniquePtr<IVulkanSurface> VulkanSwapChainPrototype::TakeSurface()
	{
		return UniquePtr<IVulkanSurface>(std::move(m_surface));
	}

	void VulkanSwapChain::SwapChainImageContainer::SetImage(VkImage image, VkImageAspectFlags allAspectFlags)
	{
		m_image = image;
		m_allAspectFlags = allAspectFlags;
	}

	VulkanSwapChain::VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numImages, SwapChainWriteBehavior writeBehavior, VulkanQueueProxyBase &queue)
		: m_device(device)
		, m_display(display)
		, m_numBuffers(numImages)
		, m_writeBehavior(writeBehavior)
		, m_queue(queue)
		, m_simultaneousImageCount(display.GetSimultaneousImageCount())
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

	Result VulkanSwapChain::Initialize(VulkanSwapChainPrototype &prototype, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, uint32_t queueFamily)
	{
		const VulkanDeviceAPI &vkd = m_device.GetDeviceAPI();
		const VulkanInstanceAPI &vki = m_device.GetInstanceAPI();

		{
			bool isCompatible = false;
			RKIT_CHECK(prototype.CheckQueueCompatibility(isCompatible, m_queue));

			if (!isCompatible)
				return ResultCode::kInternalError;
		}

		m_surface = prototype.TakeSurface();

		if (!m_surface.IsValid())
			return ResultCode::kInternalError;

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

		VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

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

		RKIT_CHECK(m_rtvs.Resize(imageCount));
		RKIT_CHECK(m_imageResources.Resize(imageCount));

		RKIT_CHECK(m_swapChainFrames.Resize(imageCount));

		for (uint32_t fi = 0; fi < imageCount; fi++)
		{
			RKIT_CHECK(m_swapChainFrames[fi].Initialize(fi));
		}

		RKIT_VK_CHECK(vkd.vkGetSwapchainImagesKHR(m_device.GetDevice(), m_swapChain, &imageCount, m_images.GetBuffer()));

		VkImageAspectFlags aspectFlags = 0;
		RKIT_CHECK(VulkanUtils::ResolveRenderTargetFormatAspectFlags(aspectFlags, fmt));

		for (uint32_t i = 0; i < imageCount; i++)
		{
			VkImageViewType imageViewType = (simultaneousImageCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			RKIT_CHECK(VulkanRenderTargetViewBase::Create(m_rtvs[i], m_device, m_images[i], swapchainCreateInfo.imageFormat, aspectFlags, imageViewType, 0, ImagePlane::kColor, 0, simultaneousImageCount));

			m_imageResources[i].SetImage(m_images[i], aspectMask);
		}

		return ResultCode::kOK;
	}

	void VulkanSwapChain::GetExtents(uint32_t &outWidth, uint32_t &outHeight) const
	{
		outWidth = m_width;
		outHeight = m_height;
	}

	Result VulkanSwapChain::AcquireFrame(ISwapChainSyncPoint &syncPointBase)
	{
		return static_cast<VulkanSwapChainSyncPoint &>(syncPointBase).AcquireFrame(m_swapChain, m_images.ToSpan(), m_swapChainFrames.ToSpan());
	}

	Result VulkanSwapChain::Present(ISwapChainSyncPoint &syncPointBase)
	{
		return static_cast<VulkanSwapChainSyncPoint &>(syncPointBase).Present(m_queue, m_swapChain);
	}

	IRenderTargetView *VulkanSwapChain::GetRenderTargetViewForFrame(size_t frameIndex)
	{
		return m_rtvs[frameIndex].Get();
	}

	IImageResource *VulkanSwapChain::GetImageForFrame(size_t frameIndex)
	{
		return &m_imageResources[frameIndex];
	}

	Result VulkanSwapChainBase::Create(UniquePtr<VulkanSwapChainBase> &outSwapChain, VulkanDeviceBase &device, VulkanSwapChainPrototypeBase &prototypeBase, uint8_t numImages, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, VulkanQueueProxyBase &queue)
	{
		VulkanSwapChainPrototype &prototype = static_cast<VulkanSwapChainPrototype &>(prototypeBase);

		uint32_t queueFamily = queue.GetQueueFamily();

		UniquePtr<VulkanSwapChain> swapChain;
		RKIT_CHECK(New<VulkanSwapChain>(swapChain, device, prototype.GetDisplay(), numImages, writeBehavior, queue));

		RKIT_CHECK(swapChain->Initialize(prototype, fmt, writeBehavior, queueFamily));

		outSwapChain = std::move(swapChain);

		return ResultCode::kOK;
	}

	Result VulkanSwapChainPrototypeBase::Create(UniquePtr<VulkanSwapChainPrototypeBase> &outSwapChainPrototype, VulkanDeviceBase &device, IDisplay &display)
	{
		UniquePtr<VulkanSwapChainPrototype> swapChainPrototype;
		RKIT_CHECK(New<VulkanSwapChainPrototype>(swapChainPrototype, device, display));

		RKIT_CHECK(swapChainPrototype->Initialize());

		outSwapChainPrototype = std::move(swapChainPrototype);

		return ResultCode::kOK;
	}

	Result VulkanSwapChainSyncPointBase::Create(UniquePtr<VulkanSwapChainSyncPointBase> &outSyncPoint, VulkanDeviceBase &device)
	{
		UniquePtr<VulkanSwapChainSyncPoint> syncPoint;
		RKIT_CHECK(New<VulkanSwapChainSyncPoint>(syncPoint, device));

		RKIT_CHECK(syncPoint->Initialize());

		outSyncPoint = std::move(syncPoint);

		return ResultCode::kOK;
	}
}
