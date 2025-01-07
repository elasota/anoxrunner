#include "rkit/Core/CoreDefs.h"

#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32

#include "VulkanPlatformSpecific.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanPlatformAPI.h"
#include "VulkanSwapChain.h"

#include "rkit/Core/Event.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/SystemDriver.h"

#include "rkit/Win32/Display_Win32.h"

namespace rkit::render::vulkan::platform
{
	class VulkanSurface_Win32 final : public IVulkanSurface
	{
	public:
		VulkanSurface_Win32(VulkanDeviceBase &device, IDisplay_Win32 &display);
		~VulkanSurface_Win32();

		Result Initialize();

		VkSurfaceKHR GetSurface() const override;

	private:
		struct WindowThreadInitTaskData
		{
			VulkanSurface_Win32 *m_surf = nullptr;
			Result m_result;
			UniquePtr<IEvent> m_doneEvent;
		};

		static void InitTaskCallback(void *userdata);

		Result WindowThreadInitialize();

		VulkanDeviceBase &m_device;
		IDisplay_Win32 &m_display;
		VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	};

	VulkanSurface_Win32::VulkanSurface_Win32(VulkanDeviceBase &device, IDisplay_Win32 &display)
		: m_device(device)
		, m_display(display)
	{
	}

	VulkanSurface_Win32::~VulkanSurface_Win32()
	{
		if (m_surface != VK_NULL_HANDLE)
			m_device.GetInstanceAPI().vkDestroySurfaceKHR(m_device.GetInstance(), m_surface, m_device.GetAllocCallbacks());
	}

	Result VulkanSurface_Win32::Initialize()
	{
		WindowThreadInitTaskData taskData;
		taskData.m_surf = this;

		RKIT_CHECK(GetDrivers().m_systemDriver->CreateEvent(taskData.m_doneEvent, false, false));

		RKIT_CHECK(m_display.PostWindowThreadTask(&taskData, VulkanSurface_Win32::InitTaskCallback));

		taskData.m_doneEvent->Wait();

		return taskData.m_result;
	}

	void VulkanSurface_Win32::InitTaskCallback(void *userdata)
	{
		WindowThreadInitTaskData *taskData = static_cast<WindowThreadInitTaskData *>(userdata);
		taskData->m_result = taskData->m_surf->WindowThreadInitialize();
		taskData->m_doneEvent->Signal();
	}

	Result VulkanSurface_Win32::WindowThreadInitialize()
	{
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.hinstance = m_display.GetHINSTANCE();
		surfaceCreateInfo.hwnd = m_display.GetHWND();

		RKIT_VK_CHECK(m_device.GetInstancePlatformAPI().vkCreateWin32SurfaceKHR(m_device.GetInstance(), &surfaceCreateInfo, m_device.GetAllocCallbacks(), &m_surface));

		return ResultCode::kOK;
	}

	VkSurfaceKHR VulkanSurface_Win32::GetSurface() const
	{
		return m_surface;
	}

	Result CreateSurfaceFromDisplay(UniquePtr<IVulkanSurface> &outSurface, VulkanDeviceBase &device, IDisplay &display)
	{
		UniquePtr<VulkanSurface_Win32> surf;
		RKIT_CHECK(New<VulkanSurface_Win32>(surf, device, static_cast<IDisplay_Win32 &>(display)));

		RKIT_CHECK(surf->Initialize());

		outSurface = std::move(surf);

		return ResultCode::kOK;
	}

	Result AddInstanceExtensions(IInstanceExtensionEnumerator &enumerator)
	{
		RKIT_CHECK(enumerator.AddExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true));

		return ResultCode::kOK;
	}

	Result AddDeviceExtensions(IDeviceExtensionEnumerator &enumerator)
	{
		return ResultCode::kOK;
	}
}

#endif
