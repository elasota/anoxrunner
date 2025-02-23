#include "VulkanCommandBatch.h"

#include "rkit/Core/HybridVector.h"

#include "rkit/Render/Barrier.h"
#include "rkit/Render/CommandEncoder.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanCommandAllocator.h"
#include "VulkanDevice.h"
#include "VulkanImageResource.h"
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

		Result WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &subsequentStages) override;
		Result SignalSwapChainPresentReady(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &priorStages) override;
		Result PipelineBarrier(const BarrierGroup &barrierGroup) override;

		Result OpenEncoder(IRenderPassInstance &rpi);

	private:
		VulkanQueueProxyBase *DefaultQueue(IBaseCommandQueue *specifiedQueue) const;

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

		Result AddWaitForVkSema(VkSemaphore sema, const PipelineStageMask_t &subsequentStageMask);
		Result AddSignalVkSema(VkSemaphore sema, const PipelineStageMask_t &priorStageMask);

		Result OpenCommandBuffer(VkCommandBuffer &outCmdBuffer);

		VulkanDeviceBase &GetDevice() const;
		VulkanQueueProxyBase &GetQueue() const;

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

	Result VulkanGraphicsCommandEncoder::WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &subsequentStages)
	{
		VkPipelineStageFlags stageFlags = 0;

		RKIT_CHECK(m_batch.AddWaitForVkSema(static_cast<VulkanSwapChainSyncPointBase &>(syncPoint).GetAcquireSema(), subsequentStages));

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::SignalSwapChainPresentReady(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &priorStages)
	{
		VkPipelineStageFlags stageFlags = 0;

		RKIT_CHECK(m_batch.AddSignalVkSema(static_cast<VulkanSwapChainSyncPointBase &>(syncPoint).GetPresentSema(), priorStages));

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::PipelineBarrier(const BarrierGroup &barrierGroup)
	{
		VkPipelineStageFlags srcStageMask = 0;
		VkPipelineStageFlags dstStageMask = 0;

		VkDependencyFlags depsFlags = 0;

		HybridVector<VkMemoryBarrier, 8> memoryBarriers;
		HybridVector<VkBufferMemoryBarrier, 8> bufferMemoryBarriers;
		HybridVector<VkImageMemoryBarrier, 8> imageMemoryBarriers;

		RKIT_CHECK(memoryBarriers.Reserve(barrierGroup.m_globalBarriers.Count()));
		RKIT_CHECK(bufferMemoryBarriers.Reserve(barrierGroup.m_bufferMemoryBarriers.Count()));
		RKIT_CHECK(imageMemoryBarriers.Reserve(barrierGroup.m_imageMemoryBarriers.Count()));

		for (const GlobalBarrier &globalBarrier : barrierGroup.m_globalBarriers)
		{
			VkMemoryBarrier memBarrier = {};
			memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;

			VkPipelineStageFlags bSrcStageMask = 0;
			VkPipelineStageFlags bDstStageMask = 0;

			RKIT_CHECK(VulkanUtils::ConvertBidirectionalPipelineStageBits(bSrcStageMask, bDstStageMask, globalBarrier.m_priorStages, globalBarrier.m_subsequentStages));
			RKIT_CHECK(VulkanUtils::ConvertResourceAccessBits(memBarrier.srcAccessMask, globalBarrier.m_priorAccess));
			RKIT_CHECK(VulkanUtils::ConvertResourceAccessBits(memBarrier.dstAccessMask, globalBarrier.m_subsequentAccess));

			srcStageMask |= bSrcStageMask;
			dstStageMask |= bDstStageMask;

			RKIT_CHECK(memoryBarriers.Append(memBarrier));
		}

		for (const BufferMemoryBarrier &bufferBarrier : barrierGroup.m_bufferMemoryBarriers)
		{
			VkBufferMemoryBarrier memBarrier = {};
			memBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

			VkPipelineStageFlags bSrcStageMask = 0;
			VkPipelineStageFlags bDstStageMask = 0;

			RKIT_CHECK(VulkanUtils::ConvertBidirectionalPipelineStageBits(bSrcStageMask, bDstStageMask, bufferBarrier.m_priorStages, bufferBarrier.m_subsequentStages));
			RKIT_CHECK(VulkanUtils::ConvertResourceAccessBits(memBarrier.srcAccessMask, bufferBarrier.m_priorAccess));
			RKIT_CHECK(VulkanUtils::ConvertResourceAccessBits(memBarrier.dstAccessMask, bufferBarrier.m_subsequentAccess));

			srcStageMask |= bSrcStageMask;
			dstStageMask |= bDstStageMask;

			memBarrier.srcQueueFamilyIndex = DefaultQueue(bufferBarrier.m_sourceQueue)->GetQueueFamily();
			memBarrier.dstQueueFamilyIndex = DefaultQueue(bufferBarrier.m_destQueue)->GetQueueFamily();

			memBarrier.offset = static_cast<VkDeviceSize>(bufferBarrier.m_offset);
			memBarrier.size = static_cast<VkDeviceSize>(bufferBarrier.m_size);

			memBarrier.buffer = 0;
			if (true)
				return ResultCode::kNotYetImplemented;

			RKIT_CHECK(bufferMemoryBarriers.Append(memBarrier));
		}

		for (const ImageMemoryBarrier &imageBarrier : barrierGroup.m_imageMemoryBarriers)
		{
			VkImageMemoryBarrier memBarrier = {};
			memBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

			VkPipelineStageFlags bSrcStageMask = 0;
			VkPipelineStageFlags bDstStageMask = 0;

			RKIT_CHECK(VulkanUtils::ConvertBidirectionalPipelineStageBits(bSrcStageMask, bDstStageMask, imageBarrier.m_priorStages, imageBarrier.m_subsequentStages));
			RKIT_CHECK(VulkanUtils::ConvertResourceAccessBits(memBarrier.srcAccessMask, imageBarrier.m_priorAccess));
			RKIT_CHECK(VulkanUtils::ConvertResourceAccessBits(memBarrier.dstAccessMask, imageBarrier.m_subsequentAccess));

			srcStageMask |= bSrcStageMask;
			dstStageMask |= bDstStageMask;

			memBarrier.srcQueueFamilyIndex = DefaultQueue(imageBarrier.m_sourceQueue)->GetQueueFamily();
			memBarrier.dstQueueFamilyIndex = DefaultQueue(imageBarrier.m_destQueue)->GetQueueFamily();

			if (imageBarrier.m_planes.IsSet())
			{
				RKIT_CHECK(VulkanUtils::ConvertImagePlaneBits(memBarrier.subresourceRange.aspectMask, imageBarrier.m_planes.Get()));
			}
			else
				memBarrier.subresourceRange.aspectMask = static_cast<VulkanImageContainer *>(imageBarrier.m_image)->GetAllAspectFlags();

			memBarrier.subresourceRange.baseMipLevel = imageBarrier.m_firstMipLevel;
			memBarrier.subresourceRange.levelCount = imageBarrier.m_numMipLevels;
			memBarrier.subresourceRange.baseArrayLayer = imageBarrier.m_firstArrayElement;
			memBarrier.subresourceRange.layerCount = imageBarrier.m_numArrayElements;

			RKIT_CHECK(VulkanUtils::ConvertImageLayout(memBarrier.oldLayout, imageBarrier.m_priorLayout));
			RKIT_CHECK(VulkanUtils::ConvertImageLayout(memBarrier.newLayout, imageBarrier.m_subsequentLayout));

			memBarrier.image = static_cast<VulkanImageContainer *>(imageBarrier.m_image)->GetVkImage();

			RKIT_CHECK(imageMemoryBarriers.Append(memBarrier));
		}

		VkCommandBuffer cmdBuffer;
		RKIT_CHECK(m_batch.OpenCommandBuffer(cmdBuffer));

		VulkanDeviceBase &device = m_batch.GetDevice();
		m_batch.GetDevice().GetDeviceAPI().vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, depsFlags,
			static_cast<uint32_t>(memoryBarriers.Count()), memoryBarriers.GetBuffer(),
			static_cast<uint32_t>(bufferMemoryBarriers.Count()), bufferMemoryBarriers.GetBuffer(),
			static_cast<uint32_t>(imageMemoryBarriers.Count()), imageMemoryBarriers.GetBuffer());

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::OpenEncoder(IRenderPassInstance &rpi)
	{
		m_rpi = &rpi;
		m_isRendering = false;

		return ResultCode::kOK;
	}

	VulkanQueueProxyBase *VulkanGraphicsCommandEncoder::DefaultQueue(IBaseCommandQueue *specifiedQueue) const
	{
		if (specifiedQueue)
			return static_cast<VulkanQueueProxyBase *>(specifiedQueue->ToInternalCommandQueue());

		return &m_batch.GetQueue();
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

	Result VulkanCommandBatch::AddWaitForVkSema(VkSemaphore sema, const PipelineStageMask_t &subsequentStages)
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
		RKIT_CHECK(VulkanUtils::ConvertPipelineStageBits(stageFlagBits, subsequentStages));

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

	Result VulkanCommandBatch::AddSignalVkSema(VkSemaphore sema, const PipelineStageMask_t &priorStages)
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


	Result VulkanCommandBatch::OpenCommandBuffer(VkCommandBuffer &outCmdBuffer)
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

		VkCommandPool pool = m_cmdAlloc.GetCommandPool();

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = pool;
		allocInfo.level = m_cmdAlloc.IsBundle() ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo, &cmdBuffer));

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkBeginCommandBuffer(cmdBuffer, &beginInfo));

		Result appendResult = m_cmdBuffers.Append(cmdBuffer);
		if (!appendResult.IsOK())
		{
			m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), pool, 1, &cmdBuffer);

			return appendResult;
		}

		outCmdBuffer = cmdBuffer;
		m_isCommandListOpen = true;

		submitItem->commandBufferCount++;

		return ResultCode::kOK;
	}

	VulkanDeviceBase &VulkanCommandBatch::GetDevice() const
	{
		return m_device;
	}

	VulkanQueueProxyBase &VulkanCommandBatch::GetQueue() const
	{
		return m_queue;
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
