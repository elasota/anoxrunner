#include "VulkanCommandAllocator.h"

#include "rkit/Core/NewDelete.h"

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
		VulkanCommandAllocator(VulkanDeviceBase &device, CommandQueueType queueType);
		~VulkanCommandAllocator();

		Result Initialize(uint32_t queueFamily);

		Result CreateCopyCommandList(UniquePtr<ICopyCommandList> &outCommandList, bool isBundle) override;
		Result CreateGraphicsCommandList(UniquePtr<IGraphicsCommandList> &outCommandList, bool isBundle) override;
		Result CreateComputeCommandList(UniquePtr<IComputeCommandList> &outCommandList, bool isBundle) override;
		Result CreateGraphicsComputeCommandList(UniquePtr<IGraphicsComputeCommandList> &outCommandList, bool isBundle) override;

		Result ResetCommandAllocator() override;

		template<class TCommandListType>
		Result CreateTypedCommandList(UniquePtr<TCommandListType> &outCommandList, bool isBundle);

		Result CreateCommandList(UniquePtr<VulkanCommandListBase> &outCommandList, bool isBundle);

	private:
		VulkanDeviceBase &m_device;
		VkCommandPool m_pool = VK_NULL_HANDLE;
		CommandQueueType m_queueType = CommandQueueType::kCount;
	};

	VulkanCommandAllocator::VulkanCommandAllocator(VulkanDeviceBase &device, CommandQueueType queueType)
		: m_device(device)
		, m_queueType(queueType)
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
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateCommandPool(m_device.GetDevice(), &poolCreateInfo, m_device.GetAllocCallbacks(), &m_pool));

		return ResultCode::kOK;
	}

	Result VulkanCommandAllocator::CreateCopyCommandList(UniquePtr<ICopyCommandList> &outCommandList, bool isBundle)
	{
		return CreateTypedCommandList(outCommandList, isBundle);
	}

	Result VulkanCommandAllocator::CreateGraphicsCommandList(UniquePtr<IGraphicsCommandList> &outCommandList, bool isBundle)
	{
		return CreateTypedCommandList(outCommandList, isBundle);
	}

	Result VulkanCommandAllocator::CreateComputeCommandList(UniquePtr<IComputeCommandList> &outCommandList, bool isBundle)
	{
		return CreateTypedCommandList(outCommandList, isBundle);
	}

	Result VulkanCommandAllocator::CreateGraphicsComputeCommandList(UniquePtr<IGraphicsComputeCommandList> &outCommandList, bool isBundle)
	{
		return CreateTypedCommandList(outCommandList, isBundle);
	}

	Result VulkanCommandAllocator::ResetCommandAllocator()
	{
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkResetCommandPool(m_device.GetDevice(), m_pool, 0));

		return ResultCode::kOK;
	}

	template<class TCommandListType>
	Result VulkanCommandAllocator::CreateTypedCommandList(UniquePtr<TCommandListType> &outCommandList, bool isBundle)
	{
		UniquePtr<VulkanCommandListBase> cmdList;

		RKIT_CHECK(CreateCommandList(cmdList, isBundle));

		outCommandList = cmdList.StaticCast<TCommandListType>();

		return ResultCode::kOK;
	}

	Result VulkanCommandAllocator::CreateCommandList(UniquePtr<VulkanCommandListBase> &outCommandList, bool isBundle)
	{
		return VulkanCommandListBase::Create(outCommandList, m_device, m_pool, m_queueType, isBundle);
	}

	Result VulkanCommandAllocatorBase::Create(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, VulkanDeviceBase &device, CommandQueueType queueType, uint32_t queueFamily)
	{
		UniquePtr<VulkanCommandAllocator> cmdAllocator;

		RKIT_CHECK(New<VulkanCommandAllocator>(cmdAllocator, device, queueType));

		RKIT_CHECK(cmdAllocator->Initialize(queueFamily));

		outCommandAllocator = std::move(cmdAllocator);

		return ResultCode::kOK;
	}
}
