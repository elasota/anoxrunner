#pragma once

#include "IncludeVulkan.h"

#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/PipelineStage.h"

namespace rkit
{
	struct Result;

	template<class T>
	class EnumMask;
}

namespace rkit::render::vulkan
{
	class VulkanUtils
	{
	public:
		static Result ResolveRenderTargetFormat(VkFormat &outVkFormat, RenderTargetFormat rtFormat);
		static Result ConvertPipelineStageBits(VkPipelineStageFlags &outFlags, const EnumMask<PipelineStage> &stages);
	};
}
