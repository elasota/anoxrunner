#include "VulkanCommandList.h"

namespace rkit::render::vulkan
{
	CommandListProxy::CommandListProxy()
	{
	}

	const VkCommandBuffer &CommandListProxy::GetCommandBuffer() const
	{
		return m_cmdBuffer;
	}
}
