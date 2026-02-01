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
				if (!resolver.IsExtensionEnabled(AsciiStringView::FromCString(loaderInfo.m_requiredExtension)))
					continue;
			}

			bool foundFunction = resolver.ResolveProc(loaderInfo.m_pfnAddress, loaderInfo, loaderInfo.m_fnName);

			if (!foundFunction && !loaderInfo.m_isOptional)
			{
				rkit::log::ErrorFmt(u8"Failed to find required Vulkan function {}", loaderInfo.m_fnName.ToUTF8());
				return ResultCode::kModuleLoadFailed;
			}
		}

		return ResultCode::kOK;
	}
} } } // rkit::render::vulkan
