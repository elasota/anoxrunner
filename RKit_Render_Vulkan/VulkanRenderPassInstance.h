#pragma once

#include "rkit/Core/CoreDefs.h"

#include "rkit/Render/RenderPassInstance.h"
#include "rkit/Render/PipelineLibraryItemProtos.h"

#include "IncludeVulkan.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct RenderPassResources;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanRenderPassInstanceBase : public IRenderPassInstance
	{
	public:
		virtual VkImageAspectFlags GetImageAspectFlagsForRTV(size_t index) const = 0;
		virtual VkImageAspectFlags GetImageAspectFlagsForDSV() const = 0;
		virtual uint32_t GetDSVAttachmentIndex() const = 0;
		virtual VkRenderPass GetVkRenderPass() const = 0;
		virtual VkFramebuffer GetVkFramebuffer() const = 0;
		virtual VkRect2D GetRenderArea() const = 0;

		static Result Create(UniquePtr<VulkanRenderPassInstanceBase> &renderPassInstance, VulkanDeviceBase &device, const RenderPassRef_t &renderPassRef, const RenderPassResources &resources);
	};
}
