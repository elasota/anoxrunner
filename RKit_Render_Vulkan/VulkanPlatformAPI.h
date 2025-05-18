#pragma once

#include "VulkanPlatformSpecific.h"

#include "VulkanAPI_Common.h"

#include "IncludeVulkanPlatformSpecific.h"

namespace rkit { namespace render { namespace vulkan
{
	struct VulkanGlobalPlatformAPI final : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanGlobalPlatformAPI);
		RKIT_VK_API_LABEL(EndPlatformSpecific);
#if 0
#else
		RKIT_VK_API_GOTO(EndPlatformSpecific);
		RKIT_VK_API_LABEL(BeginPlatformSpecific);
#endif
		RKIT_VK_API_GOTO(BeginPlatformSpecific);
		RKIT_VK_API_END;
	};

	struct VulkanInstancePlatformAPI final : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanInstancePlatformAPI);
		RKIT_VK_API_LABEL(EndPlatformSpecific);
#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32
		RKIT_VK_API_GOTO(EndPlatformSpecific);
		RKIT_VK_API_EXT(vkCreateWin32SurfaceKHR, VK_KHR_win32_surface);
		RKIT_VK_API_LABEL(BeginPlatformSpecific);
#else
		RKIT_VK_API_GOTO(EndPlatformSpecific);
		RKIT_VK_API_LABEL(BeginPlatformSpecific);
#endif
		RKIT_VK_API_GOTO(BeginPlatformSpecific);
		RKIT_VK_API_END;
	};

	struct VulkanDevicePlatformAPI final : public IVulkanAPI
	{
		RKIT_VK_API_START(VulkanDevicePlatformAPI);
		RKIT_VK_API_LABEL(EndPlatformSpecific);
#if 0
#else
		RKIT_VK_API_GOTO(EndPlatformSpecific);
		RKIT_VK_API_LABEL(BeginPlatformSpecific);
#endif
		RKIT_VK_API_GOTO(BeginPlatformSpecific);
		RKIT_VK_API_END;
	};
} } } // rkit::render::vulkan
