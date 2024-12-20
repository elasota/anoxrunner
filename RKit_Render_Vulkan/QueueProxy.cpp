#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

#include "VulkanAPI.h"
#include "VulkanCommandList.h"
#include "VulkanQueueProxy.h"

namespace rkit::render::vulkan
{
	void QueueProxy::Init(VkQueue queue, const VulkanDeviceAPI &deviceAPI)
	{
		m_vkd = &deviceAPI;
		m_queue = queue;
	}

	Result QueueProxy::ExecuteCopy(const Span<ICopyCommandList *> &cmdLists)
	{
		return ExecuteGeneric(cmdLists);
	}

	Result QueueProxy::ExecuteCompute(const Span<IComputeCommandList *> &cmdLists)
	{
		return ExecuteGeneric(cmdLists);
	}

	Result QueueProxy::ExecuteCompute(const Span<IGraphicsCommandList *> &cmdLists)
	{
		return ExecuteGeneric(cmdLists);
	}

	Result QueueProxy::ExecuteGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists)
	{
		return ExecuteGeneric(cmdLists);
	}

	template<class T>
	Result QueueProxy::ExecuteGeneric(const Span<T *> &cmdLists)
	{
		if (cmdLists.Count() > 16)
			return ExecuteGeneric<T, 32>(cmdLists);
		if (cmdLists.Count() > 8)
			return ExecuteGeneric<T, 16>(cmdLists);
		return ExecuteGeneric<T, 8>(cmdLists);
	}

	template<class T, size_t TCapacity>
	Result QueueProxy::ExecuteGenericSized(const Span<T *> &cmdListsSpan)
	{
		VkCommandBuffer cmdBuffers[TCapacity];

		T *const *cmdLists = cmdListsSpan.Ptr();
		size_t cmdListsRemaining = cmdListsSpan.Count();

		while (cmdListsRemaining > 0)
		{
			size_t thisBatchSize = cmdListsRemaining;
			if (thisBatchSize > TCapacity)
				thisBatchSize = TCapacity;

			for (size_t i = 0; i < thisBatchSize; i++)
			{
				IBaseCommandList *bcl = cmdLists[i];

				cmdBuffers[i] = static_cast<CommandListProxy *>(bcl->ToGraphicsComputeCommandList())->GetCommandBuffer();
			}

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.pCommandBuffers = cmdBuffers;
			submitInfo.commandBufferCount = static_cast<uint32_t>(thisBatchSize);

			submitInfo.

			cmdLists -= thisBatchSize;
			cmdListsRemaining -= thisBatchSize;
		}

		return ResultCode::kOK;
	}
}
