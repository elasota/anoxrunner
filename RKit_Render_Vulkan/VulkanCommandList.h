#pragma once

#include "rkit/Render/CommandList.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	class CommandListProxy final : public IGraphicsComputeCommandList
	{
	public:
		CommandListProxy();

		const VkCommandBuffer &GetCommandBuffer() const;

	private:
		VkCommandBuffer m_cmdBuffer;
	};
}
