#pragma once

#include "rkit/Render/CommandQueue.h"

#include "rkit/Core/ResizableRingBuffer.h"
#include "rkit/Core/Optional.h"

#include "IncludeVulkan.h"
#include "VulkanSync.h"

namespace rkit::render
{
	struct IBaseCommandList;
}

namespace rkit::render::vulkan
{
	struct VulkanDeviceAPI;

	class QueueProxy final : public IGraphicsComputeCommandQueue, public IInternalCommandQueue
	{
	public:
		QueueProxy(IMallocDriver *alloc, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI, TimelineID_t timelineID);
		~QueueProxy();

		Result QueueCopy(const Span<ICopyCommandList *> &cmdLists) override;
		Result QueueCompute(const Span<IComputeCommandList *> &cmdLists) override;
		Result QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists) override;
		Result QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists) override;

		Result QueueSignalGPUWaitable(GPUWaitableFence_t &outFence) override;
		Result QueueSignalCPUWaitable(CPUWaitableFence_t &outFence) override;
		Result QueueWaitFor(const GPUWaitableFence_t &gpuFence) override;

		IInternalCommandQueue *ToInternalCommandQueue() override;

		Result AddBinarySemaSignal(VkSemaphore sema, VkPipelineStageFlagBits stageFlags);
		Result AddBinarySemaWait(VkSemaphore sema, VkPipelineStageFlagBits stageFlags);
		Result Flush();

		VkQueue GetVkQueue() const;

		uint32_t GetQueueFamily() const;

	private:
		Result QueueBase(IBaseCommandList &cmdList);

		IMallocDriver *m_alloc;
		VulkanDeviceBase &m_device;
		VkQueue m_queue;
		uint32_t m_queueFamily;
		const VulkanDeviceAPI &m_vkd;

		TimelineID_t m_timelineID;
	};
}
