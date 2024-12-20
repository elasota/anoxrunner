#pragma once

#include "rkit/Core/StringProto.h"

namespace rkit
{
	struct Result;
}

namespace rkit::render::vulkan
{
	struct IVulkanAPI;
	struct FunctionLoaderInfo;

	struct IFunctionResolver
	{
		virtual bool ResolveProc(void *pfnAddr, const FunctionLoaderInfo &fli, const StringView &name) const = 0;
		virtual bool IsExtensionEnabled(const StringView &ext) const = 0;
	};

	Result LoadVulkanAPI(IVulkanAPI &api, const IFunctionResolver &resolver);
}
