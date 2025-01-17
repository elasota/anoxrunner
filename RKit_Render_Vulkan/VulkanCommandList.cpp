#include "VulkanCommandList.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"

#include "rkit/Core/NewDelete.h"

namespace rkit::render::vulkan
{
	class VulkanCommandList final : public VulkanCommandListBase
	{
	public:
		VulkanCommandList(VulkanDeviceBase &device, VkCommandPool commandPool, CommandQueueType queueType, bool isBundle);
		~VulkanCommandList();

		Result Initialize();

		IComputeCommandList *ToComputeCommandList() override;
		IGraphicsCommandList *ToGraphicsCommandList() override;
		IGraphicsComputeCommandList *ToGraphicsComputeCommandList() override;
		ICopyCommandList *ToCopyCommandList() override;

		bool IsBundle() const override;

		IInternalCommandList *ToInternalCommandList() override;

		VkCommandBuffer GetCommandBuffer() const override;

		Result ResetCommandList() override;

	private:
		VulkanDeviceBase &m_device;
		CommandQueueType m_queueType = CommandQueueType::kCount;
		VkCommandPool m_cmdPool = VK_NULL_HANDLE;
		VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
		bool m_isBundle = false;
	};

	VulkanCommandList::VulkanCommandList(VulkanDeviceBase &device, VkCommandPool commandPool, CommandQueueType queueType, bool isBundle)
		: m_device(device)
		, m_queueType(queueType)
		, m_cmdPool(commandPool)
		, m_isBundle(isBundle)
	{
	}

	VulkanCommandList::~VulkanCommandList()
	{
		if (m_cmdBuffer != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkFreeCommandBuffers(m_device.GetDevice(), m_cmdPool, 1, &m_cmdBuffer);
	}

	Result VulkanCommandList::Initialize()
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_cmdPool;
		allocInfo.level = (m_isBundle ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo, &cmdBuffer));

		m_cmdBuffer = cmdBuffer;

		return ResultCode::kOK;
	}

	IComputeCommandList *VulkanCommandList::ToComputeCommandList()
	{
		switch (m_queueType)
		{
		case CommandQueueType::kAsyncCompute:
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	IGraphicsCommandList *VulkanCommandList::ToGraphicsCommandList()
	{
		switch (m_queueType)
		{
		case CommandQueueType::kGraphics:
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	IGraphicsComputeCommandList *VulkanCommandList::ToGraphicsComputeCommandList()
	{
		switch (m_queueType)
		{
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	ICopyCommandList *VulkanCommandList::ToCopyCommandList()
	{
		switch (m_queueType)
		{
		case CommandQueueType::kCopy:
		case CommandQueueType::kAsyncCompute:
		case CommandQueueType::kGraphics:
		case CommandQueueType::kGraphicsCompute:
			return this;
		default:
			return nullptr;
		}
	}

	bool VulkanCommandList::IsBundle() const
	{
		return m_isBundle;
	}

	Result VulkanCommandList::ResetCommandList()
	{
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkResetCommandBuffer(m_cmdBuffer, 0));

		return ResultCode::kOK;
	}

	IInternalCommandList *VulkanCommandList::ToInternalCommandList()
	{
		return this;
	}

	VkCommandBuffer VulkanCommandList::GetCommandBuffer() const
	{
		return m_cmdBuffer;
	}

	Result VulkanCommandListBase::Create(UniquePtr<VulkanCommandListBase> &outCommandList, VulkanDeviceBase &device, VkCommandPool commandPool, CommandQueueType queueType, bool isBundle)
	{
		UniquePtr<VulkanCommandList> cmdList;
		RKIT_CHECK(New<VulkanCommandList>(cmdList, device, commandPool, queueType, isBundle));

		RKIT_CHECK(cmdList->Initialize());

		outCommandList = std::move(cmdList);

		return ResultCode::kOK;
	}
}
