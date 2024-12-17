#pragma once

#include "rkit/Render/RenderDevice.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render::vulkan
{
	struct VulkanDeviceBase : public IRenderDevice
	{
		static Result CreateDevice(UniquePtr<IRenderDevice> &outDevice, VkInstance inst, VkDevice device);
	};
}
