#include "VulkanUtils.h"

#include "rkit/Render/ImageRect.h"

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

	Result VulkanUtils::ResolveRenderTargetFormatAspectFlags(VkImageAspectFlags &outFlags, RenderTargetFormat rtFormat)
	{
		switch (rtFormat)
		{
		case RenderTargetFormat::RGBA_UNorm8:
		case RenderTargetFormat::RGBA_UNorm8_sRGB:
			outFlags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	Result VulkanUtils::ConvertBidirectionalPipelineStageBits(VkPipelineStageFlags &outSrcFlags, VkPipelineStageFlags &outDstFlags, const EnumMask<PipelineStage> &srcStages, const EnumMask<PipelineStage> &dstStages)
	{
		PipelineStageMask_t srcStagesCopy = srcStages;
		PipelineStageMask_t dstStagesCopy = dstStages;

		if (dstStagesCopy.Get(PipelineStage::kPresentSubmit))
		{
			dstStagesCopy.Set(PipelineStage::kPresentSubmit, false);
			dstStagesCopy.Set(PipelineStage::kBottomOfPipe, true);
		}

		if (srcStagesCopy.Get(PipelineStage::kPresentAcquire))
		{
			srcStagesCopy.Set(PipelineStage::kPresentAcquire, false);

			srcStagesCopy = (srcStagesCopy | dstStagesCopy);
		}

		RKIT_CHECK(ConvertPipelineStageBits(outSrcFlags, srcStagesCopy));
		RKIT_CHECK(ConvertPipelineStageBits(outDstFlags, dstStagesCopy));

		return ResultCode::kOK;
	}

	Result VulkanUtils::ConvertResourceAccessBits(VkAccessFlags &outFlags, const EnumMask<ResourceAccess> &accesses)
	{
		VkAccessFlags flags = 0;

		for (ResourceAccess access : accesses)
		{
			switch (access)
			{
			case ResourceAccess::kRenderTarget:
				flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			case ResourceAccess::kPresentSource:
				break;
			default:
				return ResultCode::kInternalError;
			}
		}

		outFlags = flags;

		return ResultCode::kOK;
	}

	Result VulkanUtils::ConvertImagePlaneBits(VkImageAspectFlags &outFlags, const EnumMask<ImagePlane> &planes)
	{
		VkImageAspectFlags flags = 0;

		for (ImagePlane plane : planes)
		{
			switch (plane)
			{
			case ImagePlane::kColor:
				flags |= VK_IMAGE_ASPECT_COLOR_BIT;
				break;
			case ImagePlane::kDepth:
				flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
				break;
			case ImagePlane::kStencil:
				flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
				break;

			default:
				return ResultCode::kInternalError;
			}
		}

		outFlags = flags;

		return ResultCode::kOK;
	}

	Result VulkanUtils::ConvertImageLayout(VkImageLayout &outLayout, ImageLayout imageLayout)
	{
		switch (imageLayout)
		{
		case ImageLayout::Undefined:
			outLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			break;
		case ImageLayout::RenderTarget:
			outLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;
		case ImageLayout::PresentSource:
			outLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			break;
		default:
			return ResultCode::kInternalError;
		}

		return ResultCode::kOK;
	}

	VkRect2D VulkanUtils::ConvertImageRect(const ImageRect2D &rect)
	{
		VkRect2D result;
		result.offset.x = rect.m_x;
		result.offset.y = rect.m_y;
		result.extent.width = rect.m_width;
		result.extent.height = rect.m_height;
		return result;
	}

	Result VulkanUtils::ConvertPipelineStageBits(VkPipelineStageFlags &outFlags, const EnumMask<PipelineStage> &stages)
	{
		VkPipelineStageFlags flags = 0;

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

		outFlags = flags;

		return ResultCode::kOK;
	}
}
