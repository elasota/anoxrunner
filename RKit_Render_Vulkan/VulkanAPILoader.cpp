#include "VulkanAPILoader.h"
#include "VulkanAPI.h"

#include "rkit/Core/LogDriver.h"

namespace rkit { namespace render { namespace vulkan
{
	Result LoadVulkanAPI(IVulkanAPI &api, const IFunctionResolver &resolver)
	{
		FunctionLoaderInfo loaderInfo;
		loaderInfo.m_nextCallback = api.GetFirstResolveFunctionCallback();

		while (loaderInfo.m_nextCallback != nullptr)
		{
			loaderInfo.m_nextCallback(&api, loaderInfo);

			if (loaderInfo.m_requiredExtension != nullptr)
			{
				if (!resolver.IsExtensionEnabled(StringView::FromCString(loaderInfo.m_requiredExtension)))
					continue;
			}

			bool foundFunction = resolver.ResolveProc(loaderInfo.m_pfnAddress, loaderInfo, loaderInfo.m_fnName);

			if (!foundFunction && !loaderInfo.m_isOptional)
			{
				rkit::log::ErrorFmt("Failed to find required Vulkan function %s", loaderInfo.m_fnName.GetChars());
				return ResultCode::kModuleLoadFailed;
			}
		}

		return ResultCode::kOK;
	}
} } } // rkit::render::vulkan
