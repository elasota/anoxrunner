#include "VulkanAPI.h"

namespace rkit::render::vulkan
{
	void VulkanAPI::TerminalResolveFunctionLoader(FunctionLoaderInfo &loaderInfo)
	{
		loaderInfo.m_nextCallback = nullptr;
	}
}
