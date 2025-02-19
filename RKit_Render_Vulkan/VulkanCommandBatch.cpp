#include "VulkanCommandBatch.h"

#include "rkit/Render/CommandEncoder.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanQueueProxy.h"
#include "VulkanSwapChain.h"
#include "VulkanUtils.h"

#include "rkit/Core/StaticArray.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

namespace rkit::render::vulkan
{
	class VulkanCommandBatch;

	class VulkanCommandEncoder
	{
	public:
		virtual Result CloseEncoder() = 0;
	};

	class VulkanGraphicsCommandEncoder : public IGraphicsCommandEncoder, public VulkanCommandEncoder
	{
	public:
		explicit VulkanGraphicsCommandEncoder(VulkanCommandBatch &batch);

		Result CloseEncoder() override;

		Result WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &afterStages) override;

		Result OpenEncoder(IRenderPassInstance &rpi);

	private:
		VulkanCommandBatch &m_batch;

		IRenderPassInstance *m_rpi = nullptr;
		bool m_isRendering = false;
	};

	class VulkanCommandBatch : public VulkanCommandBatchBase
	{
	public:
		enum class EncoderType
		{
			kGraphics,
			kCopy,
			kCompute,

			kCount,

			kNone = kCount,
		};

		explicit VulkanCommandBatch(VulkanDeviceBase &device, VulkanQueueProxyBase &queue, VulkanCommandAllocatorBase &cmdAlloc);
		~VulkanCommandBatch();

		Result ClearCommandBatch() override;
		Result OpenCommandBatch(bool cpuWaitable) override;

		Result Submit() override;
		Result WaitForCompletion() override;
		Result CloseBatch() override;

		Result OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder) override;
		Result OpenComputeCommandEncoder(IComputeCommandEncoder *&outCopyCommandEncoder) override;
		Result OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outCopyCommandEncoder, IRenderPassInstance &rpi) override;

		Result Initialize();

		Result StartNewEncoder(EncoderType encoderType);

		Result AddWaitForVkSema(VkSemaphore sema, const PipelineStageMask_t &afterStageMask);
		Result AddSignalVkSema(VkSemaphore sema);

	private:
		static const size_t kNumEncoderTypes = static_cast<size_t>(EncoderType::kCount);

		Result CreateNewSubmitItem(VkSubmitInfo *&outSubmitBatchItem);

		Result CheckCloseCommandList();

		VulkanDeviceBase &m_device;
		VulkanQueueProxyBase &m_queue;
		VulkanCommandAllocatorBase &m_cmdAlloc;

		Vector<VkCommandBuffer> m_cmdBuffers;

		Vector<VkSemaphore> m_semas;
		Vector<VkPipelineStageFlags> m_waitDstStageMasks;

		Vector<VkSubmitInfo> m_submits;
		bool m_isCommandListOpen = false;

		VkFence m_completionFence = VK_NULL_HANDLE;
		bool m_isCPUWaitable = false;

		VulkanGraphicsCommandEncoder m_graphicsCommandEncoder;

		StaticArray<VulkanCommandEncoder *, kNumEncoderTypes> m_encoders;
		EncoderType m_currentEncoderType = EncoderType::kNone;
	};

	VulkanGraphicsCommandEncoder::VulkanGraphicsCommandEncoder(VulkanCommandBatch &batch)
		: m_batch(batch)
	{
	}

	Result VulkanGraphicsCommandEncoder::CloseEncoder()
	{
		if (m_isRendering)
			return ResultCode::kNotYetImplemented;

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &afterStages)
	{
		VkPipelineStageFlags stageFlags = 0;

		RKIT_CHECK(m_batch.AddWaitForVkSema(static_cast<VulkanSwapChainSyncPointBase &>(syncPoint).GetAcquireSema(), afterStages));

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::OpenEncoder(IRenderPassInstance &rpi)
	{
		m_rpi = &rpi;
		m_isRendering = false;

		return ResultCode::kOK;
	}

	VulkanCommandBatch::VulkanCommandBatch(VulkanDeviceBase &device, VulkanQueueProxyBase &queue, VulkanCommandAllocatorBase &cmdAlloc)
		: m_device(device)
		, m_queue(queue)
		, m_cmdAlloc(cmdAlloc)
		, m_graphicsCommandEncoder(*this)
	{
		m_encoders[static_cast<size_t>(EncoderType::kGraphics)] = &m_graphicsCommandEncoder;
	}

	VulkanCommandBatch::~VulkanCommandBatch()
	{
		if (m_completionFence != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyFence(m_device.GetDevice(), m_completionFence, m_device.GetAllocCallbacks());
	}

	Result VulkanCommandBatch::ClearCommandBatch()
	{
		m_cmdBuffers.ShrinkToSize(0);

		m_semas.ShrinkToSize(0);
		m_waitDstStageMasks.ShrinkToSize(0);

		m_submits.ShrinkToSize(0);

		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::OpenCommandBatch(bool cpuWaitable)
	{
		if (cpuWaitable)
		{
			if (m_completionFence == VK_NULL_HANDLE)
			{
				VkFenceCreateInfo fenceCreateInfo = {};
				fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

				RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateFence(m_device.GetDevice(), &fenceCreateInfo, m_device.GetAllocCallbacks(), &m_completionFence));
			}
			else
			{
				RKIT_VK_CHECK(m_device.GetDeviceAPI().vkResetFences(m_device.GetDevice(), 1, &m_completionFence));
			}
		}

		m_isCPUWaitable = cpuWaitable;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::Submit()
	{
		RKIT_CHECK(CloseBatch());

		if (m_submits.Count() == 0 && m_completionFence == VK_NULL_HANDLE)
			return ResultCode::kOK;

		const VkCommandBuffer *cmdBuffers = m_cmdBuffers.GetBuffer();
		const VkSemaphore *semas = m_semas.GetBuffer();
		const VkPipelineStageFlags *waitStageFlags = m_waitDstStageMasks.GetBuffer();

		for (VkSubmitInfo &submitInfo : m_submits)
		{
			if (submitInfo.commandBufferCount > 0)
			{
				submitInfo.pCommandBuffers = cmdBuffers;
				cmdBuffers += submitInfo.commandBufferCount;
			}

			if (submitInfo.waitSemaphoreCount > 0)
			{
				submitInfo.pWaitSemaphores = semas;
				submitInfo.pWaitDstStageMask = waitStageFlags;

				semas += submitInfo.waitSemaphoreCount;
				waitStageFlags += submitInfo.waitSemaphoreCount;
			}

			if (submitInfo.signalSemaphoreCount > 0)
			{
				submitInfo.pSignalSemaphores = semas;

				semas += submitInfo.signalSemaphoreCount;
			}
		}

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkQueueSubmit(m_queue.GetVkQueue(), static_cast<uint32_t>(m_submits.Count()), m_submits.GetBuffer(), m_completionFence));

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::WaitForCompletion()
	{
		if (!m_isCPUWaitable)
			return ResultCode::kInternalError;

		if (m_completionFence == VK_NULL_HANDLE)
			return ResultCode::kInternalError;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkWaitForFences(m_device.GetDevice(), 1, &m_completionFence, VK_TRUE, UINT64_MAX));

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::CloseBatch()
	{
		RKIT_CHECK(StartNewEncoder(EncoderType::kNone));
		RKIT_CHECK(CheckCloseCommandList());

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::OpenComputeCommandEncoder(IComputeCommandEncoder *&outComputeCommandEncoder)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outGraphicsCommandEncoder, IRenderPassInstance &rpi)
	{
		RKIT_CHECK(StartNewEncoder(EncoderType::kGraphics));

		RKIT_CHECK(m_graphicsCommandEncoder.OpenEncoder(rpi));

		outGraphicsCommandEncoder = &m_graphicsCommandEncoder;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::AddWaitForVkSema(VkSemaphore sema, const PipelineStageMask_t &afterStageMask)
	{
		VkSubmitInfo *submitItem = nullptr;
		if (m_submits.Count() > 0)
		{
			RKIT_CHECK(CheckCloseCommandList());

			submitItem = &m_submits[m_submits.Count() - 1];
			if (submitItem->commandBufferCount > 0 || submitItem->signalSemaphoreCount > 0)
				submitItem = nullptr;
		}

		if (!submitItem)
		{
			RKIT_CHECK(CreateNewSubmitItem(submitItem));
		}

		VkPipelineStageFlags stageFlagBits = 0;
		RKIT_CHECK(VulkanUtils::ConvertPipelineStageBits(stageFlagBits, afterStageMask));

		RKIT_CHECK(m_semas.Append(sema));

		{
			Result dstMaskAppendResult = m_waitDstStageMasks.Append(stageFlagBits);
			if (!dstMaskAppendResult.IsOK())
			{
				m_semas.ShrinkToSize(m_semas.Count() - 1);
				return dstMaskAppendResult;
			}
		}

		submitItem->waitSemaphoreCount++;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::AddSignalVkSema(VkSemaphore sema)
	{
		VkSubmitInfo *submitItem = nullptr;
		if (m_submits.Count() > 0)
		{
			RKIT_CHECK(CheckCloseCommandList());

			submitItem = &m_submits[m_submits.Count() - 1];
		}

		if (!submitItem)
		{
			RKIT_CHECK(CreateNewSubmitItem(submitItem));
		}

		RKIT_CHECK(m_semas.Append(sema));

		submitItem->signalSemaphoreCount++;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::CreateNewSubmitItem(VkSubmitInfo *&outSubmitInfo)
	{
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		RKIT_CHECK(m_submits.Append(submitInfo));

		outSubmitInfo = &m_submits[m_submits.Count() - 1];

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::CheckCloseCommandList()
	{
		if (m_isCommandListOpen)
		{
			RKIT_VK_CHECK(m_device.GetDeviceAPI().vkEndCommandBuffer(m_cmdBuffers[m_cmdBuffers.Count() - 1]));

			m_isCommandListOpen = false;
		}

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::Initialize()
	{
		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::StartNewEncoder(EncoderType encoderType)
	{
		if (m_currentEncoderType != EncoderType::kNone)
		{
			RKIT_CHECK(m_encoders[static_cast<size_t>(m_currentEncoderType)]->CloseEncoder());
		}

		m_currentEncoderType = encoderType;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatchBase::Create(UniquePtr<VulkanCommandBatchBase> &outCmdBatch, VulkanDeviceBase &device, VulkanQueueProxyBase &queue, VulkanCommandAllocatorBase &cmdAlloc)
	{
		UniquePtr<VulkanCommandBatch> cmdBatch;
		RKIT_CHECK(New<VulkanCommandBatch>(cmdBatch, device, queue, cmdAlloc));

		RKIT_CHECK(cmdBatch->Initialize());

		outCmdBatch = std::move(cmdBatch);

		return ResultCode::kOK;
	}
};
