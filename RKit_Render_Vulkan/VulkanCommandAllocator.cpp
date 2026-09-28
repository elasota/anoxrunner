#include "VulkanCommandAllocator.h"
#include "VulkanCommandBatch.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
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

		Result OpenCopyCommandBatch(ICopyCommandBatch *&outCommandBatch) override;
		Result OpenGraphicsCommandBatch(IGraphicsCommandBatch *&outCommandBatch) override;
		Result OpenComputeCommandBatch(IComputeCommandBatch *&outCommandBatch) override;
		Result OpenGraphicsComputeCommandBatch(IGraphicsComputeCommandBatch *&outCommandBatch) override;

		Result ResetCommandAllocator(bool discardResources) override;

		CommandQueueType GetQueueType() const override;
		bool IsBundle() const override;
		VkCommandPool GetCommandPool() const override;
		Result AcquireCommandBuffer(VkCommandBuffer &outCmdBuffer) override;

		DynamicCastRef_t InternalDynamicCast() override;

		template<class TCommandBatchType>
		Result TypedOpenCommandBatch(TCommandBatchType *&outCommandBath);

		Result OpenCommandBatch(VulkanCommandBatchBase *&outCommandBatch);

	private:
		Vector<UniquePtr<VulkanCommandBatchBase>> m_commandBatches;
		size_t m_numAllocatedBatches = 0;

		VulkanDeviceBase &m_device;
		VulkanQueueProxyBase &m_queue;
		VkCommandPool m_pool = VK_NULL_HANDLE;
		CommandQueueType m_queueType = CommandQueueType::kCount;
		bool m_isBundle = false;

		Vector<VkCommandBuffer> m_cmdBuffers;
		size_t m_activeCmdBuffers = 0;
	};
}

namespace rkit
{
	template<>
	struct DynamicDowncaster<render::vulkan::VulkanCommandAllocator, render::ICopyCommandAllocator>
	{
		static render::ICopyCommandAllocator *Cast(render::vulkan::VulkanCommandAllocator *src);
	};
}

namespace rkit::render::vulkan
{
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

		RKIT_RETURN_OK;
	}

	IInternalCommandAllocator *VulkanCommandAllocator::ToInternalCommandAllocator()
	{
		return this;
	}

	Result VulkanCommandAllocator::OpenCopyCommandBatch(ICopyCommandBatch *&outCommandBatch)
	{
		IComputeCommandBatch *intermediateBatch = nullptr;
		TypedOpenCommandBatch(intermediateBatch);

		outCommandBatch = intermediateBatch;
	}

	Result VulkanCommandAllocator::OpenGraphicsCommandBatch(IGraphicsCommandBatch *&outCommandBatch)
	{
		return TypedOpenCommandBatch(outCommandBatch);
	}

	Result VulkanCommandAllocator::OpenComputeCommandBatch(IComputeCommandBatch *&outCommandBatch)
	{
		return TypedOpenCommandBatch(outCommandBatch);
	}

	Result VulkanCommandAllocator::OpenGraphicsComputeCommandBatch(IGraphicsComputeCommandBatch *&outCommandBatch)
	{
		return TypedOpenCommandBatch(outCommandBatch);
	}

	Result VulkanCommandAllocator::ResetCommandAllocator(bool discardResources)
	{
		VkCommandPoolResetFlags resetFlags = 0;

		if (discardResources)
			resetFlags |= VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkResetCommandPool(m_device.GetDevice(), m_pool, resetFlags));

		m_numAllocatedBatches = 0;

		if (discardResources)
		{
			if (m_cmdBuffers.Count() > 0)
				m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), m_pool, static_cast<uint32_t>(m_cmdBuffers.Count()), m_cmdBuffers.GetBuffer());

			m_cmdBuffers.Reset();
			m_commandBatches.Reset();
		}

		m_activeCmdBuffers = 0;

		RKIT_RETURN_OK;
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

	Result VulkanCommandAllocator::AcquireCommandBuffer(VkCommandBuffer &outCmdBuffer)
	{
		if (m_activeCmdBuffers < m_cmdBuffers.Count())
		{
			outCmdBuffer = m_cmdBuffers[m_activeCmdBuffers];
			m_activeCmdBuffers++;

			RKIT_RETURN_OK;
		}

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandBufferCount = 1;
		allocInfo.commandPool = m_pool;
		allocInfo.level = m_isBundle ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo, &cmdBuffer));

		RKIT_TRY_CATCH_RETHROW(m_cmdBuffers.Append(cmdBuffer),
			CatchContext(
				[this, cmdBuffer]
				{
					m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), m_pool, 1, &cmdBuffer);
				}
			)
		);

		outCmdBuffer = cmdBuffer;

		m_activeCmdBuffers++;

		RKIT_RETURN_OK;
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
	Result VulkanCommandAllocator::TypedOpenCommandBatch(TCommandBatchType *&outCommandBatch)
	{
		VulkanCommandBatchBase *cmdBatch = nullptr;

		OpenCommandBatch(cmdBatch);

		outCommandBatch = cmdBatch;

		RKIT_RETURN_OK;
	}

	Result VulkanCommandAllocator::OpenCommandBatch(VulkanCommandBatchBase *&outCommandBatch)
	{
		VulkanCommandBatchBase *cmdBatchPtr = nullptr;

		if (m_numAllocatedBatches == m_commandBatches.Count())
		{
			UniquePtr<VulkanCommandBatchBase> cmdBatch;

			VulkanCommandBatchBase::Create(cmdBatch, m_device, m_queue, *this);

			cmdBatchPtr = cmdBatch.Get();

			cmdBatchPtr->OpenCommandBatch();

			m_commandBatches.Append(std::move(cmdBatch));
			m_numAllocatedBatches++;
		}
		else
		{
			cmdBatchPtr = m_commandBatches[m_numAllocatedBatches].Get();

			cmdBatchPtr->ResetCommandBatch();

			cmdBatchPtr->OpenCommandBatch();

			m_numAllocatedBatches++;
		}

		outCommandBatch = cmdBatchPtr;

		RKIT_RETURN_OK;
	}

	Result VulkanCommandAllocatorBase::Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, VulkanQueueProxyBase &queue, CommandQueueType queueType, bool isBundle, uint32_t queueFamily)
	{
		UniquePtr<VulkanCommandAllocator> cmdAllocator = New<VulkanCommandAllocator>(device, queue, queueType, isBundle);

		cmdAllocator->Initialize(queueFamily);

		outCommandAllocator = std::move(cmdAllocator);
	}
} // rkit::render::vulkan


namespace rkit
{
	inline render::ICopyCommandAllocator *DynamicDowncaster<render::vulkan::VulkanCommandAllocator, render::ICopyCommandAllocator>::Cast(render::vulkan::VulkanCommandAllocator *src)
	{
		render::IComputeCommandAllocator *computeAlloc = src;
		return computeAlloc;
	}
} // rkit
