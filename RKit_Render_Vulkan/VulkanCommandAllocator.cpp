#include "VulkanCommandAllocator.h"
#include "VulkanCommandBatch.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanCommandList.h"
#include "VulkanDevice.h"
#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	class VulkanCommandAllocator final : public VulkanCommandAllocatorBase
	{
	public:
		VulkanCommandAllocator(VulkanDeviceBase &device, VulkanQueueProxyBase &queue, CommandQueueType queueType, bool isBundle);
		~VulkanCommandAllocator();

		Result Initialize(uint32_t queueFamily);

		IInternalCommandAllocator *ToInternalCommandAllocator() override;

		Result OpenCopyCommandBatch(ICopyCommandBatch *&outCommandBatch, bool cpuWaitable) override;
		Result OpenGraphicsCommandBatch(IGraphicsCommandBatch *&outCommandBatch, bool cpuWaitable) override;
		Result OpenComputeCommandBatch(IComputeCommandBatch *&outCommandBatch, bool cpuWaitable) override;
		Result OpenGraphicsComputeCommandBatch(IGraphicsComputeCommandBatch *&outCommandBatch, bool cpuWaitable) override;

		Result ResetCommandAllocator(bool discardResources) override;

		CommandQueueType GetQueueType() const override;
		bool IsBundle() const override;
		VkCommandPool GetCommandPool() const override;

		DynamicCastRef_t InternalDynamicCast() override;

		template<class TCommandBatchType>
		Result TypedOpenCommandBatch(TCommandBatchType *&outCommandBath, bool cpuWaitable);

		Result OpenCommandBatch(VulkanCommandBatchBase *&outCommandBatch, bool cpuWaitable);

	private:
		Vector<UniquePtr<VulkanCommandBatchBase>> m_commandBatches;
		size_t m_numAllocatedBatches = 0;

		VulkanDeviceBase &m_device;
		VulkanQueueProxyBase &m_queue;
		VkCommandPool m_pool = VK_NULL_HANDLE;
		CommandQueueType m_queueType = CommandQueueType::kCount;
		bool m_isBundle = false;

		Vector<VulkanCommandList> m_commandLists;
		size_t m_activeCommandLists = 0;
	};

	VulkanCommandAllocator::VulkanCommandAllocator(VulkanDeviceBase &device, VulkanQueueProxyBase &queue, CommandQueueType queueType, bool isBundle)
		: m_device(device)
		, m_queue(queue)
		, m_queueType(queueType)
		, m_isBundle(isBundle)
	{
	}

	VulkanCommandAllocator::~VulkanCommandAllocator()
	{
		if (m_pool != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyCommandPool(m_device.GetDevice(), m_pool, m_device.GetAllocCallbacks());
	}

	Result VulkanCommandAllocator::Initialize(uint32_t queueFamily)
	{
		VkCommandPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.queueFamilyIndex = queueFamily;
		poolCreateInfo.flags = 0;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateCommandPool(m_device.GetDevice(), &poolCreateInfo, m_device.GetAllocCallbacks(), &m_pool));

		return ResultCode::kOK;
	}

	IInternalCommandAllocator *VulkanCommandAllocator::ToInternalCommandAllocator()
	{
		return this;
	}

	Result VulkanCommandAllocator::OpenCopyCommandBatch(ICopyCommandBatch *&outCommandBatch, bool cpuWaitable)
	{
		return TypedOpenCommandBatch(outCommandBatch, cpuWaitable);
	}

	Result VulkanCommandAllocator::OpenGraphicsCommandBatch(IGraphicsCommandBatch *&outCommandBatch, bool cpuWaitable)
	{
		return TypedOpenCommandBatch(outCommandBatch, cpuWaitable);
	}

	Result VulkanCommandAllocator::OpenComputeCommandBatch(IComputeCommandBatch *&outCommandBatch, bool cpuWaitable)
	{
		return TypedOpenCommandBatch(outCommandBatch, cpuWaitable);
	}

	Result VulkanCommandAllocator::OpenGraphicsComputeCommandBatch(IGraphicsComputeCommandBatch *&outCommandBatch, bool cpuWaitable)
	{
		return TypedOpenCommandBatch(outCommandBatch, cpuWaitable);
	}

	Result VulkanCommandAllocator::ResetCommandAllocator(bool discardResources)
	{
		VkCommandPoolResetFlags resetFlags = 0;

		if (discardResources)
			resetFlags |= VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkResetCommandPool(m_device.GetDevice(), m_pool, resetFlags));

		if (discardResources)
		{
			const size_t kMaxFreeGrouping = 16;

			VkCommandBuffer buffersToFree[kMaxFreeGrouping];
			uint32_t numQueuedFrees = 0;

			for (const VulkanCommandList &cmdList : m_commandLists)
			{
				buffersToFree[numQueuedFrees++] = cmdList.GetCommandBuffer();
				if (numQueuedFrees == kMaxFreeGrouping)
				{
					m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), m_pool, numQueuedFrees, buffersToFree);
					numQueuedFrees = 0;
				}
			}

			if (numQueuedFrees > 0)
				m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), m_pool, numQueuedFrees, buffersToFree);

			m_commandLists.Reset();

			m_activeCommandLists = 0;
		}
		else
		{
			while (m_activeCommandLists > 0)
			{
				m_activeCommandLists--;
				RKIT_VK_CHECK(m_device.GetDeviceAPI().vkResetCommandBuffer(m_commandLists[m_activeCommandLists].GetCommandBuffer(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));
			}
		}

		return ResultCode::kOK;
	}

	CommandQueueType VulkanCommandAllocator::GetQueueType() const
	{
		return m_queueType;
	}

	bool VulkanCommandAllocator::IsBundle() const
	{
		return m_isBundle;
	}

	VkCommandPool VulkanCommandAllocator::GetCommandPool() const
	{
		return m_pool;
	}

	VulkanCommandAllocator::DynamicCastRef_t VulkanCommandAllocator::InternalDynamicCast()
	{
		switch (m_queueType)
		{
		case CommandQueueType::kGraphics:
			return DynamicCastRef_t::CreateFrom<VulkanCommandAllocator, IGraphicsCommandAllocator, ICopyCommandAllocator>(this);
		case CommandQueueType::kGraphicsCompute:
			return DynamicCastRef_t::CreateFrom<VulkanCommandAllocator, IGraphicsCommandAllocator, IComputeCommandAllocator, IGraphicsComputeCommandAllocator, ICopyCommandAllocator>(this);
		case CommandQueueType::kAsyncCompute:
			return DynamicCastRef_t::CreateFrom<VulkanCommandAllocator, IComputeCommandAllocator, ICopyCommandAllocator>(this);
		case CommandQueueType::kCopy:
			return DynamicCastRef_t::CreateFrom<VulkanCommandAllocator, ICopyCommandAllocator>(this);
		default:
			return DynamicCastRef_t();
		}
	}

	template<class TCommandBatchType>
	Result VulkanCommandAllocator::TypedOpenCommandBatch(TCommandBatchType *&outCommandBatch, bool cpuWaitable)
	{
		VulkanCommandBatchBase *cmdBatch = nullptr;

		RKIT_CHECK(OpenCommandBatch(cmdBatch, cpuWaitable));

		outCommandBatch = cmdBatch;

		return ResultCode::kOK;
	}

	Result VulkanCommandAllocator::OpenCommandBatch(VulkanCommandBatchBase *&outCommandBatch, bool cpuWaitable)
	{
		VulkanCommandBatchBase *cmdBatchPtr = nullptr;

		if (m_numAllocatedBatches == m_commandBatches.Count())
		{
			UniquePtr<VulkanCommandBatchBase> cmdBatch;

			RKIT_CHECK(VulkanCommandBatchBase::Create(cmdBatch, m_device, m_queue, *this));

			cmdBatchPtr = cmdBatch.Get();

			RKIT_CHECK(cmdBatchPtr->OpenCommandBatch(cpuWaitable));

			RKIT_CHECK(m_commandBatches.Append(std::move(cmdBatch)));
			m_numAllocatedBatches++;
		}
		else
		{
			cmdBatchPtr = m_commandBatches[m_numAllocatedBatches].Get();

			RKIT_CHECK(cmdBatchPtr->ClearCommandBatch());

			RKIT_CHECK(cmdBatchPtr->OpenCommandBatch(cpuWaitable));

			m_numAllocatedBatches++;
		}

		outCommandBatch = cmdBatchPtr;

		return ResultCode::kOK;
	}

	Result VulkanCommandAllocatorBase::Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, VulkanQueueProxyBase &queue, CommandQueueType queueType, bool isBundle, uint32_t queueFamily)
	{
		UniquePtr<VulkanCommandAllocator> cmdAllocator;

		RKIT_CHECK(New<VulkanCommandAllocator>(cmdAllocator, device, queue, queueType, isBundle));

		RKIT_CHECK(cmdAllocator->Initialize(queueFamily));

		outCommandAllocator = std::move(cmdAllocator);

		return ResultCode::kOK;
	}
}
