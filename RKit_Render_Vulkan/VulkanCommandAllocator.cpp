#include "VulkanCommandAllocator.h"

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
		VulkanCommandAllocator(VulkanDeviceBase &device, CommandQueueType queueType, bool isBundle);
		~VulkanCommandAllocator();

		Result Initialize(uint32_t queueFamily);

		Result OpenCopyCommandList(ICopyCommandList *&outCommandList) override;
		Result OpenGraphicsCommandList(IGraphicsCommandList *&outCommandList) override;
		Result OpenComputeCommandList(IComputeCommandList *&outCommandList) override;
		Result OpenGraphicsComputeCommandList(IGraphicsComputeCommandList *&outCommandList) override;

		Result ResetCommandAllocator(bool discardResources) override;

		CommandQueueType GetQueueType() const override;
		bool IsBundle() const override;

		template<class TCommandListType>
		Result CreateTypedCommandList(TCommandListType *&outCommandList);

		Result CreateCommandList(VulkanCommandList *&outCommandList);

	private:
		VulkanDeviceBase &m_device;
		VkCommandPool m_pool = VK_NULL_HANDLE;
		CommandQueueType m_queueType = CommandQueueType::kCount;
		bool m_isBundle = false;

		Vector<VulkanCommandList> m_commandLists;
		size_t m_activeCommandLists = 0;
	};

	VulkanCommandAllocator::VulkanCommandAllocator(VulkanDeviceBase &device, CommandQueueType queueType, bool isBundle)
		: m_device(device)
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

	Result VulkanCommandAllocator::OpenCopyCommandList(ICopyCommandList *&outCommandList)
	{
		return CreateTypedCommandList(outCommandList);
	}

	Result VulkanCommandAllocator::OpenGraphicsCommandList(IGraphicsCommandList *&outCommandList)
	{
		return CreateTypedCommandList(outCommandList);
	}

	Result VulkanCommandAllocator::OpenComputeCommandList(IComputeCommandList *&outCommandList)
	{
		return CreateTypedCommandList(outCommandList);
	}

	Result VulkanCommandAllocator::OpenGraphicsComputeCommandList(IGraphicsComputeCommandList *&outCommandList)
	{
		return CreateTypedCommandList(outCommandList);
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

	template<class TCommandListType>
	Result VulkanCommandAllocator::CreateTypedCommandList(TCommandListType *&outCommandList)
	{
		VulkanCommandList *cmdList;

		RKIT_CHECK(CreateCommandList(cmdList));

		outCommandList = cmdList;

		return ResultCode::kOK;
	}

	Result VulkanCommandAllocator::CreateCommandList(VulkanCommandList *&outCommandList)
	{
		if (m_activeCommandLists < m_commandLists.Count())
		{
			outCommandList = &m_commandLists[m_activeCommandLists++];
			return ResultCode::kOK;
		}

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_pool;
		allocInfo.level = (m_isBundle ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo, &cmdBuffer));

		Result appendResult = m_commandLists.Append(VulkanCommandList(m_device, cmdBuffer, *this));
		if (!appendResult.IsOK())
		{
			m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), m_pool, 1, &cmdBuffer);
			return appendResult;
		}

		outCommandList = &m_commandLists[m_activeCommandLists++];

		return ResultCode::kOK;
	}

	Result VulkanCommandAllocatorBase::Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, CommandQueueType queueType, bool isBundle, uint32_t queueFamily)
	{
		UniquePtr<VulkanCommandAllocator> cmdAllocator;

		RKIT_CHECK(New<VulkanCommandAllocator>(cmdAllocator, device, queueType, isBundle));

		RKIT_CHECK(cmdAllocator->Initialize(queueFamily));

		outCommandAllocator = std::move(cmdAllocator);

		return ResultCode::kOK;
	}
}
