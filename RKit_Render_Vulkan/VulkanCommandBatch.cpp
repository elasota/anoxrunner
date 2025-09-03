#include "VulkanCommandBatch.h"

#include "rkit/Core/HybridVector.h"
#include "rkit/Core/Result.h"

#include "rkit/Render/Barrier.h"
#include "rkit/Render/BufferImageFootprint.h"
#include "rkit/Render/CommandEncoder.h"
#include "rkit/Render/DepthStencilTargetClear.h"
#include "rkit/Render/ImageRect.h"
#include "rkit/Render/RenderTargetClear.h"

#include "VulkanAPI.h"
#include "VulkanBufferResource.h"
#include "VulkanCheck.h"
#include "VulkanCommandAllocator.h"
#include "VulkanDevice.h"
#include "VulkanImageResource.h"
#include "VulkanQueueProxy.h"
#include "VulkanRenderPassInstance.h"
#include "VulkanSwapChain.h"
#include "VulkanUtils.h"

#include "rkit/Core/StaticArray.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

namespace rkit { namespace render { namespace vulkan
{
	class VulkanCommandBatch;

	class VulkanCommandEncoder
	{
	public:
		virtual Result CloseEncoder() = 0;
	};

	class VulkanCopyCommandEncoder : public ICopyCommandEncoder, public VulkanCommandEncoder
	{
	public:
		explicit VulkanCopyCommandEncoder(VulkanCommandBatch &batch);

		Result CopyBufferToImage(IImageResource &imageResource, const ImageRect3D &destRect,
			IBufferResource &bufferResource, const BufferImageFootprint &bufferFootprint,
			ImageLayout imageLayout, uint32_t mipLevel, uint32_t arrayLayer, ImagePlane plane) override;

		Result CloseEncoder() override;

	private:
		VulkanCommandBatch &m_batch;
	};

	class VulkanGraphicsCommandEncoder : public IGraphicsCommandEncoder, public VulkanCommandEncoder
	{
	public:
		explicit VulkanGraphicsCommandEncoder(VulkanCommandBatch &batch);

		Result CloseEncoder() override;

		Result WaitForSwapChainAcquire(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &subsequentStages) override;
		Result SignalSwapChainPresentReady(ISwapChainSyncPoint &syncPoint, const rkit::EnumMask<rkit::render::PipelineStage> &priorStages) override;
		Result PipelineBarrier(const BarrierGroup &barrierGroup) override;
		Result ClearTargets(const Span<const RenderTargetClear> &renderTargetClears, const DepthStencilTargetClear *depthStencilClear, const Span<const ImageRect2D> &rects) override;

		Result OpenEncoder(IRenderPassInstance &rpi);

	private:
		VulkanQueueProxyBase *DefaultQueue(IBaseCommandQueue *specifiedQueue) const;

		VulkanCommandBatch &m_batch;

		VulkanRenderPassInstanceBase *m_rpi = nullptr;
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

		Result ResetCommandBatch() override;
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
		Result OpenRenderPass(VkCommandBuffer &outCmdBuffer, const VulkanRenderPassInstanceBase &rpi);
		Result CloseRenderPass();

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
		bool m_isRenderPassOpen = false;

		VkFence m_completionFence = VK_NULL_HANDLE;
		bool m_isCPUWaitable = false;

		VulkanCopyCommandEncoder m_copyCommandEncoder;
		VulkanGraphicsCommandEncoder m_graphicsCommandEncoder;

		StaticArray<VulkanCommandEncoder *, kNumEncoderTypes> m_encoders;
		EncoderType m_currentEncoderType = EncoderType::kNone;
	};


	VulkanCopyCommandEncoder::VulkanCopyCommandEncoder(VulkanCommandBatch &batch)
		: m_batch(batch)
	{
	}

	Result VulkanCopyCommandEncoder::CopyBufferToImage(IImageResource &imageResource, const ImageRect3D &destRect,
		IBufferResource &bufferResource, const BufferImageFootprint &bufferFootprint,
		ImageLayout imageLayout, uint32_t mipLevel, uint32_t arrayLayer, ImagePlane plane)
	{
		VkImageLayout vkImageLayout;
		RKIT_CHECK(VulkanUtils::ConvertImageLayout(vkImageLayout, imageLayout));

		VkImageAspectFlags aspectMask = 0;
		RKIT_CHECK(VulkanUtils::ConvertImagePlaneBits(aspectMask, ImagePlaneMask_t({ plane })));

		uint32_t blockSizeBytes = 0;
		uint32_t blockWidth = 0;
		uint32_t blockHeight = 0;
		uint32_t blockDepth = 0;
		RKIT_CHECK(VulkanUtils::GetTextureFormatCharacteristics(bufferFootprint.m_format, blockSizeBytes, blockWidth, blockHeight, blockDepth));

		RKIT_ASSERT(bufferFootprint.m_rowPitch % blockSizeBytes == 0);

		VkBufferImageCopy bufferImageCopy = {};
		bufferImageCopy.bufferOffset = bufferFootprint.m_bufferOffset;
		bufferImageCopy.bufferRowLength = bufferFootprint.m_rowPitch / blockSizeBytes * blockWidth;
		bufferImageCopy.bufferImageHeight = bufferFootprint.m_height;
		bufferImageCopy.imageSubresource.aspectMask = aspectMask;
		bufferImageCopy.imageSubresource.mipLevel = mipLevel;
		bufferImageCopy.imageSubresource.baseArrayLayer = arrayLayer;
		bufferImageCopy.imageSubresource.layerCount = 1;
		bufferImageCopy.imageOffset.x = destRect.m_x;
		bufferImageCopy.imageOffset.y = destRect.m_y;
		bufferImageCopy.imageOffset.z = destRect.m_z;
		bufferImageCopy.imageExtent.width = destRect.m_width;
		bufferImageCopy.imageExtent.height = destRect.m_height;
		bufferImageCopy.imageExtent.depth = destRect.m_depth;

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		RKIT_CHECK(m_batch.OpenCommandBuffer(cmdBuffer));

		VulkanDeviceBase &device = m_batch.GetDevice();
		device.GetDeviceAPI().vkCmdCopyBufferToImage(cmdBuffer,
			static_cast<VulkanBuffer &>(bufferResource).GetVkBuffer(),
			static_cast<VulkanImageContainer &>(imageResource).GetVkImage(),
			vkImageLayout, 1, &bufferImageCopy);

		return ResultCode::kOK;
	}

	Result VulkanCopyCommandEncoder::CloseEncoder()
	{
		return ResultCode::kOK;
	}

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

		RKIT_CHECK(m_batch.CloseRenderPass());

		VkCommandBuffer cmdBuffer;
		RKIT_CHECK(m_batch.OpenCommandBuffer(cmdBuffer));

		VulkanDeviceBase &device = m_batch.GetDevice();
		m_batch.GetDevice().GetDeviceAPI().vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, depsFlags,
			static_cast<uint32_t>(memoryBarriers.Count()), memoryBarriers.GetBuffer(),
			static_cast<uint32_t>(bufferMemoryBarriers.Count()), bufferMemoryBarriers.GetBuffer(),
			static_cast<uint32_t>(imageMemoryBarriers.Count()), imageMemoryBarriers.GetBuffer());

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::ClearTargets(const Span<const RenderTargetClear> &renderTargetClears, const DepthStencilTargetClear *depthStencilClear, const Span<const ImageRect2D> &rects)
	{
		const VulkanDeviceBase &device = m_batch.GetDevice();

		HybridVector<VkClearAttachment, 8> clearAttachments;
		HybridVector<VkClearRect, 8> clearRects;

		const size_t numRects = rects.Count();
		const size_t numRTs = renderTargetClears.Count();
		const size_t numDSTs = (depthStencilClear == nullptr) ? 0 : 1;

		const RenderTargetClear *rtClears = renderTargetClears.Ptr();
		const DepthStencilTargetClear *dtClears = depthStencilClear;
		const ImageRect2D *rectsPtr = rects.Ptr();


		RKIT_CHECK(clearRects.Resize(numRects));
		RKIT_CHECK(clearAttachments.Resize(numRTs + numDSTs));

		for (size_t i = 0; i < numRects; i++)
		{
			const ImageRect2D &rect = rectsPtr[i];
			VkClearRect &clearRect = clearRects[i];

			clearRect = {};
			clearRect.layerCount = 1;
			clearRect.rect.offset.x = rect.m_x;
			clearRect.rect.offset.y = rect.m_x;
			clearRect.rect.extent.width = rect.m_height;
			clearRect.rect.extent.height = rect.m_width;
		}

		for (size_t i = 0; i < numRTs; i++)
		{
			const RenderTargetClear &rtClear = rtClears[i];
			VkClearAttachment &clearAttachment = clearAttachments[i];

			clearAttachment = {};
			clearAttachment.aspectMask = m_rpi->GetImageAspectFlagsForRTV(rtClear.m_renderTargetIndex);

			static_assert(sizeof(rtClear.m_clearValue) == sizeof(clearAttachment.clearValue.color));

			memcpy(&clearAttachment.clearValue.color, &rtClear.m_clearValue, sizeof(clearAttachment.clearValue));

			clearAttachment.colorAttachment = rtClear.m_renderTargetIndex;
		}

		if (depthStencilClear)
		{
			VkClearAttachment &clearAttachment = clearAttachments[numRTs];

			clearAttachment = {};

			if (!depthStencilClear->m_planes.IsSet())
				clearAttachment.aspectMask = m_rpi->GetImageAspectFlagsForDSV();
			else
			{
				RKIT_CHECK(VulkanUtils::ConvertImagePlaneBits(clearAttachment.aspectMask, depthStencilClear->m_planes.Get()));
			}

			clearAttachment.colorAttachment = m_rpi->GetDSVAttachmentIndex();
			clearAttachment.clearValue.depthStencil.depth = depthStencilClear->m_depth;
			clearAttachment.clearValue.depthStencil.stencil = depthStencilClear->m_stencil;
		}

		VkCommandBuffer cmdBuffer;
		RKIT_CHECK(m_batch.OpenRenderPass(cmdBuffer, *m_rpi));

		device.GetDeviceAPI().vkCmdClearAttachments(cmdBuffer, static_cast<uint32_t>(clearAttachments.Count()), clearAttachments.GetBuffer(), static_cast<uint32_t>(clearRects.Count()), clearRects.GetBuffer());

		return ResultCode::kOK;
	}

	Result VulkanGraphicsCommandEncoder::OpenEncoder(IRenderPassInstance &rpi)
	{
		m_rpi = static_cast<VulkanRenderPassInstanceBase *>(&rpi);
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
		, m_copyCommandEncoder(*this)
		, m_graphicsCommandEncoder(*this)
	{
		m_encoders[static_cast<size_t>(EncoderType::kGraphics)] = &m_graphicsCommandEncoder;
		m_encoders[static_cast<size_t>(EncoderType::kCopy)] = &m_copyCommandEncoder;
	}

	VulkanCommandBatch::~VulkanCommandBatch()
	{
		if (m_completionFence != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyFence(m_device.GetDevice(), m_completionFence, m_device.GetAllocCallbacks());
	}

	Result VulkanCommandBatch::ResetCommandBatch()
	{
		m_cmdBuffers.ShrinkToSize(0);

		m_semas.ShrinkToSize(0);
		m_waitDstStageMasks.ShrinkToSize(0);

		m_submits.ShrinkToSize(0);

		if (m_isCPUWaitable)
		{
			RKIT_ASSERT(m_completionFence != VK_NULL_HANDLE);
			m_device.GetDeviceAPI().vkResetFences(m_device.GetDevice(), 1, &m_completionFence);

			m_isCPUWaitable = false;
		}

		return ResultCode::kOK;
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
		RKIT_CHECK(StartNewEncoder(EncoderType::kCopy));

		outCopyCommandEncoder = &m_copyCommandEncoder;

		return ResultCode::kOK;
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
			if (!utils::ResultIsOK(dstMaskAppendResult))
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
		if (m_isCommandListOpen)
		{
			outCmdBuffer = m_cmdBuffers[m_cmdBuffers.Count() - 1];
			return ResultCode::kOK;
		}

		VkSubmitInfo *submitItem = nullptr;
		if (m_submits.Count() > 0)
		{
			submitItem = &m_submits[m_submits.Count() - 1];
			if (submitItem->signalSemaphoreCount > 0)
				submitItem = nullptr;
		}

		if (!submitItem)
		{
			RKIT_CHECK(CreateNewSubmitItem(submitItem));
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		RKIT_CHECK(m_cmdAlloc.AcquireCommandBuffer(cmdBuffer));

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkBeginCommandBuffer(cmdBuffer, &beginInfo));

		RKIT_CHECK(m_cmdBuffers.Append(cmdBuffer));

		outCmdBuffer = cmdBuffer;
		m_isCommandListOpen = true;

		submitItem->commandBufferCount++;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::OpenRenderPass(VkCommandBuffer &outCmdBuffer, const VulkanRenderPassInstanceBase &rpi)
	{
		VkCommandBuffer cmdBuffer;

		RKIT_CHECK(OpenCommandBuffer(cmdBuffer));

		if (!m_isRenderPassOpen)
		{
			VkRenderPassBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			beginInfo.renderPass = rpi.GetVkRenderPass();
			beginInfo.framebuffer = rpi.GetVkFramebuffer();
			beginInfo.renderArea = rpi.GetRenderArea();

			m_device.GetDeviceAPI().vkCmdBeginRenderPass(cmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

			m_isRenderPassOpen = true;
		}

		outCmdBuffer = cmdBuffer;

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

	Result VulkanCommandBatch::CloseRenderPass()
	{
		if (m_isRenderPassOpen)
		{
			RKIT_ASSERT(m_isCommandListOpen);
			m_device.GetDeviceAPI().vkCmdEndRenderPass(m_cmdBuffers[m_cmdBuffers.Count() - 1]);

			m_isRenderPassOpen = false;
		}

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::CheckCloseCommandList()
	{
		if (m_isCommandListOpen)
		{
			RKIT_CHECK(CloseRenderPass());

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
} } } // rkit::render::vulkan
