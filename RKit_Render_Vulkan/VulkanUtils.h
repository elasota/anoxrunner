#pragma once

#include "IncludeVulkan.h"

#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/ImagePlane.h"
#include "rkit/Render/PipelineStage.h"
#include "rkit/Render/ResourceAccess.h"

namespace rkit
{
	template<class T>
	class EnumMask;

	namespace render
	{
		struct ImageRect2D;
	}
}

namespace rkit { namespace render { namespace vulkan
{
	class VulkanUtils
	{
	public:
		static Result ResolveRenderTargetFormat(VkFormat &outVkFormat, RenderTargetFormat rtFormat);
		static Result ResolveRenderTargetFormatAspectFlags(VkImageAspectFlags &outFlags, RenderTargetFormat rtFormat);
		static Result ConvertPipelineStageBits(VkPipelineStageFlags &outFlags, const EnumMask<PipelineStage> &stages);
		static Result ConvertBidirectionalPipelineStageBits(VkPipelineStageFlags &outSrcFlags, VkPipelineStageFlags &outDstFlags, const EnumMask<PipelineStage> &srcStages, const EnumMask<PipelineStage> &dstStages);
		static Result ConvertResourceAccessBits(VkAccessFlags &outFlags, const EnumMask<ResourceAccess> &accesses);
		static Result ConvertImagePlaneBits(VkImageAspectFlags &outFlags, const EnumMask<ImagePlane> &planes);
		static Result ConvertImageLayout(VkImageLayout &outLayout, ImageLayout imageLayout);
		static VkRect2D ConvertImageRect(const ImageRect2D &rect);
		static Result GetTextureFormatCharacteristics(TextureFormat textureFormat, uint32_t &outBlockSizeBytes, uint32_t &outBlockWidth, uint32_t &outBlockHeight, uint32_t &outBlockDepth);
	};
} } } // rkit::render::vulkan
