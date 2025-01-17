#include "VulkanUtils.h"

#include "rkit/Core/EnumMask.h"

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

	Result VulkanUtils::ConvertPipelineStageBits(VkPipelineStageFlags &flags, const EnumMask<PipelineStage> &stages)
	{
		flags = 0;
		for (PipelineStage stage : stages)
		{
			switch (stage)
			{
			case PipelineStage::kTopOfPipe:
				flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				break;
			case PipelineStage::kBottomOfPipe:
				flags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				break;
			case PipelineStage::kColorOutput:
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				break;
			default:
				return ResultCode::kInternalError;
			}
		}

		return ResultCode::kOK;
	}
}
