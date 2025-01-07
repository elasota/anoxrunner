#include "VulkanUtils.h"


namespace rkit::render::vulkan
{
	Result VulkanUtils::ResolveRenderTargetFormat(VkFormat &outVkFormat, RenderTargetFormat rtFormat)
	{
		switch (rtFormat)
		{
		case RenderTargetFormat::RGBA_UNorm8:
			outVkFormat = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		case RenderTargetFormat::RGBA_UNorm8_sRGB:
			outVkFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}
}
