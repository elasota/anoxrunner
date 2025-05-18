#pragma once

#include "rkit/Core/StringProto.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	namespace render
	{
		struct IDisplay;

		namespace vulkan
		{
			class VulkanDeviceBase;

			struct IVulkanSurface;
		}
	}
}

namespace rkit { namespace render { namespace vulkan { namespace platform
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
} } } } // rkit::render::vulkan::platform
