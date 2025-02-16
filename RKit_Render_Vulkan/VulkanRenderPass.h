#pragma once

#include "rkit/Render/RenderPass.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	class VulkanRenderPassBase : public IInternalRenderPass
	{
	public:
		virtual VkRenderPass GetRenderPass() const = 0;
	};
}
