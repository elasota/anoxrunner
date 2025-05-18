#pragma once

#include "rkit/Render/RenderPass.h"

#include "IncludeVulkan.h"

namespace rkit { namespace render { namespace vulkan
{
	class VulkanRenderPassBase : public IInternalRenderPass
	{
	public:
		virtual VkRenderPass GetRenderPass() const = 0;
	};
} } } // rkit::render::vulkan
