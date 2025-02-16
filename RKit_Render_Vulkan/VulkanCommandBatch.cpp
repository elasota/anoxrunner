#include "VulkanCommandBatch.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanUtils.h"

#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

namespace rkit::render::vulkan
{
	class VulkanCommandBatch : public VulkanCommandBatchBase
	{
	public:
		explicit VulkanCommandBatch(VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc);
		~VulkanCommandBatch();

		Result ClearCommandBatch() override;
		Result OpenCommandBatch(bool cpuWaitable) override;

		Result Submit() override;
		Result WaitForCompletion() override;

		Result OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder) override;
		Result OpenComputeCommandEncoder(IComputeCommandEncoder *&outCopyCommandEncoder) override;
		Result OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outCopyCommandEncoder) override;

		Result Initialize();

	private:
		enum class BatchItemType
		{
			kUnallocated,
			kSubmit,
		};

		struct SubmitBatchItem
		{
			VkSubmitInfo m_submitInfo;

			size_t m_firstSemaphore;
			size_t m_firstWaitDstStageMask;
			size_t m_firstCommandBuffer;
		};

		union BatchItemUnion
		{
			SubmitBatchItem m_submit;
		};

		struct BatchItem
		{
			BatchItemType m_type;
			BatchItemUnion m_union;
		};

		Result CmdWaitForVkSema(VkSemaphore sema, const PipelineStageMask_t &beforeStageMask);

		Result CreateNewSubmitItem(SubmitBatchItem *&outSubmitBatchItem);

		Result SubmitItem(const BatchItem &item);

		VulkanDeviceBase &m_device;
		VulkanCommandAllocatorBase &m_cmdAlloc;

		Vector<VkCommandBuffer> m_cmdBuffers;

		Vector<VkSemaphore> m_semas;
		Vector<VkPipelineStageFlags> m_waitDstStageMasks;

		Vector<BatchItem> m_items;

		VkFence m_completionFence = VK_NULL_HANDLE;
		bool m_isCPUWaitable = false;
	};

	VulkanCommandBatch::VulkanCommandBatch(VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc)
		: m_device(device)
		, m_cmdAlloc(cmdAlloc)
	{
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

		m_items.ShrinkToSize(0);

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
		return ResultCode::kNotYetImplemented;
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

	Result VulkanCommandBatch::OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::OpenComputeCommandEncoder(IComputeCommandEncoder *&outCopyCommandEncoder)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanCommandBatch::OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outCopyCommandEncoder)
	{
		return ResultCode::kNotYetImplemented;
	}


	Result VulkanCommandBatch::CmdWaitForVkSema(VkSemaphore sema, const PipelineStageMask_t &beforeStageMask)
	{
		SubmitBatchItem *submitItem = nullptr;
		if (m_items.Count() > 0)
		{
			BatchItem &lastItem = m_items[m_items.Count() - 1];
			if (lastItem.m_type == BatchItemType::kSubmit)
			{
				submitItem = &lastItem.m_union.m_submit;

				if (submitItem->m_submitInfo.commandBufferCount > 0 || submitItem->m_submitInfo.signalSemaphoreCount > 0)
					submitItem = nullptr;
			}
		}

		if (!submitItem)
		{
			RKIT_CHECK(CreateNewSubmitItem(submitItem));
		}

		VkPipelineStageFlags stageFlagBits = 0;
		RKIT_CHECK(VulkanUtils::ConvertPipelineStageBits(stageFlagBits, beforeStageMask));

		RKIT_CHECK(m_semas.Append(sema));

		{
			Result dstMaskAppendResult = m_waitDstStageMasks.Append(stageFlagBits);
			if (!dstMaskAppendResult.IsOK())
			{
				m_semas.ShrinkToSize(m_semas.Count() - 1);
				return dstMaskAppendResult;
			}
		}

		submitItem->m_submitInfo.waitSemaphoreCount++;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::CreateNewSubmitItem(SubmitBatchItem *&outSubmitBatchItem)
	{
		RKIT_CHECK(m_items.Append(BatchItem()));

		BatchItem &newItem = m_items[m_items.Count() - 1];
		newItem.m_type = BatchItemType::kUnallocated;

		SubmitBatchItem &submitItem = newItem.m_union.m_submit;
		submitItem = {};
		submitItem.m_firstCommandBuffer = m_cmdBuffers.Count();
		submitItem.m_firstSemaphore = m_semas.Count();
		submitItem.m_firstWaitDstStageMask = m_waitDstStageMasks.Count();

		VkSubmitInfo &submitInfo = submitItem.m_submitInfo;
		submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		outSubmitBatchItem = &submitItem;

		return ResultCode::kOK;
	}

	Result VulkanCommandBatch::Initialize()
	{
		return ResultCode::kOK;
	}

	Result VulkanCommandBatchBase::Create(UniquePtr<VulkanCommandBatchBase> &outCmdBatch, VulkanDeviceBase &device, VulkanCommandAllocatorBase &cmdAlloc)
	{
		UniquePtr<VulkanCommandBatch> cmdBatch;
		RKIT_CHECK(New<VulkanCommandBatch>(cmdBatch, device, cmdAlloc));

		RKIT_CHECK(cmdBatch->Initialize());

		outCmdBatch = std::move(cmdBatch);

		return ResultCode::kOK;
	}
};
