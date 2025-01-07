#pragma once

#include "IncludeVulkan.h"

#include "rkit/Render/RenderDefs.h"

namespace rkit
{
	struct Result;
}

namespace rkit::render::vulkan
{
	class VulkanUtils
	{
	public:
		static Result ResolveRenderTargetFormat(VkFormat &outVkFormat, RenderTargetFormat rtFormat);
	};
}
