#pragma once

#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct IDisplay;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	struct IVulkanSurface;
}

namespace rkit::render::vulkan::platform
{
	struct IInstanceExtensionEnumerator
	{
		virtual Result AddExtension(const StringView &ext, bool required) = 0;
		virtual Result AddLayer(const StringView &layer, bool required) = 0;
	};

	struct IDeviceExtensionEnumerator
	{
		virtual Result AddExtension(const StringView &ext, bool required) = 0;
	};

	Result AddInstanceExtensions(IInstanceExtensionEnumerator &enumerator);
	Result AddDeviceExtensions(IDeviceExtensionEnumerator &enumerator);
	Result CreateSurfaceFromDisplay(UniquePtr<IVulkanSurface> &outSurface, VulkanDeviceBase &device, IDisplay &display);
}
