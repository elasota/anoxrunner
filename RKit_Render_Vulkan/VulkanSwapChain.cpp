#include "VulkanSwapChain.h"

#include "VulkanAPI.h"
#include "VulkanDevice.h"
#include "VulkanPlatformSpecific.h"

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Result.h"

namespace rkit::render::vulkan
{
	class VulkanSwapChain final : public VulkanSwapChainBase
	{
	public:
		VulkanSwapChain(VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers);
		~VulkanSwapChain();

		Result Initialize();

	private:
		VulkanDeviceBase &m_device;
		IDisplay &m_display;
		uint8_t m_numBackBuffers = 0;

		UniquePtr<IVulkanSurface> m_surface;
		VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
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

	Result VulkanSwapChain::Initialize()
	{
		RKIT_CHECK(platform::CreateSurfaceFromDisplay(m_surface, m_device, m_display));

		return ResultCode::kNotYetImplemented;
	}

	Result VulkanSwapChainBase::Create(UniquePtr<VulkanSwapChainBase> &outSwapChain, VulkanDeviceBase &device, IDisplay &display, uint8_t numBackBuffers)
	{
		UniquePtr<VulkanSwapChain> swapChain;
		RKIT_CHECK(New<VulkanSwapChain>(swapChain, device, display, numBackBuffers));

		RKIT_CHECK(swapChain->Initialize());

		outSwapChain = std::move(swapChain);

		return ResultCode::kOK;
	}
}
