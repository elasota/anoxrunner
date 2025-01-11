#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

#include "VulkanAPI.h"
#include "VulkanDevice.h"
#include "VulkanCommandList.h"
#include "VulkanQueueProxy.h"

#include <cstring>

namespace rkit::render::vulkan
{
	QueueProxy::QueueProxy(IMallocDriver *alloc, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI, TimelineID_t timelineID)
		: m_alloc(alloc)
		, m_device(device)
		, m_queue(queue)
		, m_queueFamily(queueFamily)
		, m_vkd(deviceAPI)
		, m_timelineID(timelineID)
	{
	}

	QueueProxy::~QueueProxy()
	{
	}

	Result QueueProxy::QueueCopy(const Span<ICopyCommandList *> &cmdLists)
	{
		for (ICopyCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueCompute(const Span<IComputeCommandList *> &cmdLists)
	{
		for (IComputeCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists)
	{
		for (IGraphicsCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists)
	{
		for (IGraphicsComputeCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueSignalGPUWaitable(GPUWaitableFence_t &outFence)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result QueueProxy::QueueSignalCPUWaitable(CPUWaitableFence_t &outFence)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result QueueProxy::QueueWaitFor(const GPUWaitableFence_t &gpuFence)
	{
		return ResultCode::kNotYetImplemented;
	}

	IInternalCommandQueue *QueueProxy::ToInternalCommandQueue()
	{
		return this;
	}

	Result QueueProxy::AddBinarySemaSignal(VkSemaphore sema, VkPipelineStageFlagBits stageFlags)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result QueueProxy::AddBinarySemaWait(VkSemaphore sema, VkPipelineStageFlagBits stageFlags)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result QueueProxy::Flush()
	{
		return ResultCode::kNotYetImplemented;
	}

	VkQueue QueueProxy::GetVkQueue() const
	{
		return m_queue;
	}

	uint32_t QueueProxy::GetQueueFamily() const
	{
		return m_queueFamily;
	}

	Result QueueProxy::QueueBase(IBaseCommandList &cmdList)
	{
		return ResultCode::kNotYetImplemented;
	}
}
