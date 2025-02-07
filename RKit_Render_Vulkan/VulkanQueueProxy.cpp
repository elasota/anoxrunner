#include "VulkanQueueProxy.h"

#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanCommandList.h"
#include "VulkanFence.h"
#include "VulkanUtils.h"

#include <cstring>

namespace rkit::render::vulkan
{
	class VulkanQueueProxy final : public VulkanQueueProxyBase
	{
	public:
		VulkanQueueProxy(IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI);
		~VulkanQueueProxy();

		Result QueueCopy(const Span<ICopyCommandList *> &cmdLists) override;
		Result QueueCompute(const Span<IComputeCommandList *> &cmdLists) override;
		Result QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists) override;
		Result QueueWaitForSwapChainWriteReady(ISwapChainSyncPoint &syncPoint) override;
		Result QueueWaitForSwapChainPresentReady(ISwapChainSyncPoint &syncPoint) override;

		Result QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists) override;

		Result QueueSignalBinaryGPUWaitable(IBinaryGPUWaitableFence &fence) override;
		Result QueueSignalBinaryCPUWaitable(IBinaryCPUWaitableFence &fence) override;
		Result QueueWaitForBinaryGPUWaitable(IBinaryGPUWaitableFence &fence, const EnumMask<PipelineStage> &stagesToWaitFor) override;

		CommandQueueType GetCommandQueueType() const override;

		ICopyCommandQueue *ToCopyCommandQueue() override;
		IComputeCommandQueue *ToComputeCommandQueue() override;
		IGraphicsCommandQueue *ToGraphicsCommandQueue() override;
		IGraphicsComputeCommandQueue *ToGraphicsComputeCommandQueue() override;
		IInternalCommandQueue *ToInternalCommandQueue() override;

		Result Flush() override;
		uint32_t GetQueueFamily() const override;

		Result AddBinarySemaSignal(VkSemaphore sema) override;
		Result AddBinarySemaWait(VkSemaphore sema, VkPipelineStageFlags stageFlags) override;
		Result AddCommandList(VkCommandBuffer commandList) override;

		VkQueue GetVkQueue() const override;

		Result Initialize();

	private:
		enum class QueueAction
		{
			NonBatchable,
			CommandList,
			BinarySemaSignal,
			BinarySemaWait,
		};

		static const size_t kMaxCommandLists = 128;
		static const size_t kMaxSubmitInfos = 64;
		static const size_t kMaxWaitSemas = 64;
		static const size_t kMaxSignalSemas = 64;

		Result QueueBase(IBaseCommandList &cmdList);

		Result FlushWithFence(VkFence fence);

		StaticArray<VkCommandBuffer, kMaxCommandLists> m_commandLists;

		StaticArray<VkSemaphore, kMaxSignalSemas> m_signalSemas;

		StaticArray<VkSemaphore, kMaxWaitSemas> m_waitSemas;
		StaticArray<VkPipelineStageFlags, kMaxWaitSemas> m_waitSemaStageMasks;

		StaticArray<VkSubmitInfo, kMaxSubmitInfos> m_submitInfos;

		size_t m_numCommandLists = 0;
		size_t m_numSubmitInfos = 0;
		size_t m_numWaitSemas = 0;
		size_t m_numSignalSemas = 0;

		QueueAction m_lastQueueAction = QueueAction::NonBatchable;

		IMallocDriver *m_alloc;
		CommandQueueType m_queueType;
		VulkanDeviceBase &m_device;
		VkQueue m_queue;
		uint32_t m_queueFamily;
		const VulkanDeviceAPI &m_vkd;
	};

	VulkanQueueProxy::VulkanQueueProxy(IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI)
		: m_alloc(alloc)
		, m_queueType(queueType)
		, m_device(device)
		, m_queue(queue)
		, m_queueFamily(queueFamily)
		, m_vkd(deviceAPI)
	{
	}

	VulkanQueueProxy::~VulkanQueueProxy()
	{
	}

	Result VulkanQueueProxy::QueueCopy(const Span<ICopyCommandList *> &cmdLists)
	{
		for (ICopyCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result VulkanQueueProxy::QueueCompute(const Span<IComputeCommandList *> &cmdLists)
	{
		for (IComputeCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result VulkanQueueProxy::QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists)
	{
		for (IGraphicsCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result VulkanQueueProxy::QueueWaitForSwapChainWriteReady(ISwapChainSyncPoint &syncPoint)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanQueueProxy::QueueWaitForSwapChainPresentReady(ISwapChainSyncPoint &syncPoint)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanQueueProxy::QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists)
	{
		for (IGraphicsComputeCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result VulkanQueueProxy::QueueSignalBinaryGPUWaitable(IBinaryGPUWaitableFence &fence)
	{
		return AddBinarySemaSignal(static_cast<VulkanBinaryGPUWaitableFence &>(fence).GetSemaphore());
	}

	Result VulkanQueueProxy::QueueSignalBinaryCPUWaitable(IBinaryCPUWaitableFence &fence)
	{
		return FlushWithFence(static_cast<VulkanBinaryCPUWaitableFence &>(fence).GetFence());
	}

	Result VulkanQueueProxy::QueueWaitForBinaryGPUWaitable(IBinaryGPUWaitableFence &fence, const EnumMask<PipelineStage> &stagesToWaitFor)
	{
		VkPipelineStageFlags stageFlags = 0;
		RKIT_CHECK(VulkanUtils::ConvertPipelineStageBits(stageFlags, stagesToWaitFor));
		return AddBinarySemaWait(static_cast<VulkanBinaryGPUWaitableFence &>(fence).GetSemaphore(), stageFlags);
	}

	CommandQueueType VulkanQueueProxy::GetCommandQueueType() const
	{
		return m_queueType;
	}

	ICopyCommandQueue *VulkanQueueProxy::ToCopyCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kCopy))
			return this;
		else
			return nullptr;
	}

	IComputeCommandQueue *VulkanQueueProxy::ToComputeCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kAsyncCompute))
			return this;
		else
			return nullptr;
	}

	IGraphicsCommandQueue *VulkanQueueProxy::ToGraphicsCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kGraphics))
			return this;
		else
			return nullptr;
	}

	IGraphicsComputeCommandQueue *VulkanQueueProxy::ToGraphicsComputeCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kGraphicsCompute))
			return this;
		else
			return nullptr;
	}

	IInternalCommandQueue *VulkanQueueProxy::ToInternalCommandQueue()
	{
		return this;
	}

	Result VulkanQueueProxy::AddBinarySemaSignal(VkSemaphore sema)
	{
		if (m_numSignalSemas == kMaxSignalSemas)
		{
			RKIT_CHECK(Flush());
		}

		switch (m_lastQueueAction)
		{
		case QueueAction::NonBatchable:
			{
				if (m_numSubmitInfos == kMaxSubmitInfos)
				{
					RKIT_CHECK(Flush());
				}

				VkSemaphore *pSemaphore = &m_signalSemas[m_numSignalSemas++];

				VkSubmitInfo &submitInfo = m_submitInfos[m_numSubmitInfos++];
				submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.waitSemaphoreCount = 0;
				submitInfo.pWaitSemaphores = nullptr;
				submitInfo.signalSemaphoreCount = 1;
				submitInfo.pSignalSemaphores = pSemaphore;
				submitInfo.pWaitDstStageMask = nullptr;
				submitInfo.commandBufferCount = 0;
				submitInfo.pCommandBuffers = 0;

				*pSemaphore = sema;
				m_lastQueueAction = QueueAction::BinarySemaSignal;
			}
			return ResultCode::kOK;
		case QueueAction::CommandList:
		case QueueAction::BinarySemaSignal:
		case QueueAction::BinarySemaWait:
			{
				VkSubmitInfo &submitInfo = m_submitInfos[m_numSubmitInfos - 1];

				VkSemaphore *pSemaphore = &m_signalSemas[m_numSignalSemas++];

				if (submitInfo.signalSemaphoreCount == 0)
					submitInfo.pSignalSemaphores = pSemaphore;

				submitInfo.signalSemaphoreCount++;
				*pSemaphore = sema;
				m_lastQueueAction = QueueAction::BinarySemaSignal;
			}
			return ResultCode::kOK;
		default:
			return ResultCode::kInternalError;
		}
	}

	Result VulkanQueueProxy::AddBinarySemaWait(VkSemaphore sema, VkPipelineStageFlags stageFlags)
	{
		if (m_numWaitSemas == kMaxWaitSemas)
		{
			RKIT_CHECK(Flush());
		}

		switch (m_lastQueueAction)
		{
		case QueueAction::NonBatchable:
		case QueueAction::CommandList:
		case QueueAction::BinarySemaSignal:
			{
				if (m_numSubmitInfos == kMaxSubmitInfos)
				{
					RKIT_CHECK(Flush());
				}

				VkSemaphore *pSemaphore = &m_waitSemas[m_numWaitSemas];
				VkPipelineStageFlags *pStageFlags = &m_waitSemaStageMasks[m_numWaitSemas];

				m_numWaitSemas++;

				VkSubmitInfo &submitInfo = m_submitInfos[m_numSubmitInfos++];
				submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.waitSemaphoreCount = 1;
				submitInfo.pWaitSemaphores = pSemaphore;
				submitInfo.pWaitDstStageMask = pStageFlags;
				submitInfo.signalSemaphoreCount = 0;
				submitInfo.pSignalSemaphores = nullptr;
				submitInfo.commandBufferCount = 0;
				submitInfo.pCommandBuffers = 0;

				*pSemaphore = sema;
				*pStageFlags = stageFlags;

				m_lastQueueAction = QueueAction::BinarySemaWait;
			}
			return ResultCode::kOK;
		case QueueAction::BinarySemaWait:
			{
				VkSubmitInfo &submitInfo = m_submitInfos[m_numSubmitInfos - 1];

				VkSemaphore *pSemaphore = &m_waitSemas[m_numWaitSemas];
				VkPipelineStageFlags *pStageFlags = &m_waitSemaStageMasks[m_numWaitSemas];

				m_numWaitSemas++;

				submitInfo.waitSemaphoreCount++;

				*pSemaphore = sema;
				*pStageFlags = stageFlags;
			}
			return ResultCode::kOK;
		default:
			return ResultCode::kInternalError;
		}
	}

	Result VulkanQueueProxy::AddCommandList(VkCommandBuffer commandList)
	{
		if (m_numCommandLists == kMaxCommandLists)
		{
			RKIT_CHECK(Flush());
		}

		switch (m_lastQueueAction)
		{
		case QueueAction::NonBatchable:
		case QueueAction::BinarySemaSignal:
			{
				if (m_numSubmitInfos == kMaxSubmitInfos)
				{
					RKIT_CHECK(Flush());
				}

				VkCommandBuffer *pCommandList = &m_commandLists[m_numCommandLists++];

				VkSubmitInfo &submitInfo = m_submitInfos[m_numSubmitInfos++];
				submitInfo = {};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.waitSemaphoreCount = 0;
				submitInfo.pWaitSemaphores = nullptr;
				submitInfo.signalSemaphoreCount = 0;
				submitInfo.pSignalSemaphores = nullptr;
				submitInfo.pWaitDstStageMask = nullptr;
				submitInfo.commandBufferCount = 0;
				submitInfo.pCommandBuffers = 0;

				*pCommandList = commandList;
				m_lastQueueAction = QueueAction::CommandList;
			}
			return ResultCode::kOK;
		case QueueAction::CommandList:
		case QueueAction::BinarySemaWait:
			{
				VkSubmitInfo &submitInfo = m_submitInfos[m_numSubmitInfos - 1];

				VkCommandBuffer *pCommandList = &m_commandLists[m_numCommandLists++];

				if (submitInfo.commandBufferCount == 0)
					submitInfo.pCommandBuffers = pCommandList;

				submitInfo.commandBufferCount++;
				*pCommandList = commandList;

				m_lastQueueAction = QueueAction::CommandList;
			}
			return ResultCode::kOK;
		default:
			return ResultCode::kInternalError;
		}
	}

	Result VulkanQueueProxy::Flush()
	{
		return FlushWithFence(VK_NULL_HANDLE);
	}

	Result VulkanQueueProxy::FlushWithFence(VkFence fence)
	{
		m_lastQueueAction = QueueAction::NonBatchable;

		if (m_numSubmitInfos > 0)
		{
			RKIT_VK_CHECK(m_device.GetDeviceAPI().vkQueueSubmit(m_queue, static_cast<uint32_t>(m_numSubmitInfos), &m_submitInfos[0], fence));

			m_numSubmitInfos = 0;
			m_numSignalSemas = 0;
			m_numWaitSemas = 0;
			m_numCommandLists = 0;
		}
		else
		{
			RKIT_ASSERT(m_numSignalSemas == 0);
			RKIT_ASSERT(m_numWaitSemas == 0);
			RKIT_ASSERT(m_numCommandLists == 0);

			if (fence != VK_NULL_HANDLE)
			{
				RKIT_VK_CHECK(m_device.GetDeviceAPI().vkQueueSubmit(m_queue, 0, nullptr, fence));
			}
		}

		return ResultCode::kOK;
	}

	VkQueue VulkanQueueProxy::GetVkQueue() const
	{
		return m_queue;
	}

	uint32_t VulkanQueueProxy::GetQueueFamily() const
	{
		return m_queueFamily;
	}

	Result VulkanQueueProxy::Initialize()
	{
		return ResultCode::kOK;
	}

	Result VulkanQueueProxy::QueueBase(IBaseCommandList &cmdList)
	{
		return AddCommandList(static_cast<VulkanCommandList *>(cmdList.ToInternalCommandList())->GetCommandBuffer());
	}

	Result VulkanQueueProxyBase::Create(UniquePtr<VulkanQueueProxyBase> &outQueueProxy, IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI)
	{
		UniquePtr<VulkanQueueProxy> queueProxy;
		RKIT_CHECK(New<VulkanQueueProxy>(queueProxy, alloc, queueType, device, queue, queueFamily, deviceAPI));

		RKIT_CHECK(queueProxy->Initialize());

		outQueueProxy = std::move(queueProxy);

		return ResultCode::kOK;
	}
}
