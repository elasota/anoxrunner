#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"

namespace rkit { namespace render { namespace vulkan
{
	struct IVulkanAPI;
	struct FunctionLoaderInfo;

	struct IFunctionResolver
	{
		virtual bool ResolveProc(void *pfnAddr, const FunctionLoaderInfo &fli, const AsciiStringView &name) const = 0;
		virtual bool IsExtensionEnabled(const AsciiStringView &ext) const = 0;
	};

	Result LoadVulkanAPI(IVulkanAPI &api, const IFunctionResolver &resolver);
} } } // rkit::render::vulkan
