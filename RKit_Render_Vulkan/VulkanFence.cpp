#include "VulkanFence.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"

namespace rkit::render::vulkan
{
	VulkanBinaryCPUWaitableFence::VulkanBinaryCPUWaitableFence(VulkanDeviceBase &device)
		: m_device(device)
		, m_fence(VK_NULL_HANDLE)
	{
	}

	VulkanBinaryCPUWaitableFence::~VulkanBinaryCPUWaitableFence()
	{
		if (m_fence != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyFence(m_device.GetDevice(), m_fence, m_device.GetAllocCallbacks());
	}

	Result VulkanBinaryCPUWaitableFence::Initialize(bool startSignaled)
	{
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		if (startSignaled)
			fenceCreateInfo.flags |= VK_FENCE_CREATE_SIGNALED_BIT;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateFence(m_device.GetDevice(), &fenceCreateInfo, m_device.GetAllocCallbacks(), &m_fence));
		return ResultCode::kOK;
	}

	Result VulkanBinaryCPUWaitableFence::WaitFor()
	{
		return this->WaitForTimed(std::numeric_limits<uint64_t>::max());
	}

	Result VulkanBinaryCPUWaitableFence::WaitForTimed(uint64_t timeoutMSec)
	{
		IBinaryCPUWaitableFence *fence = this;
		return m_device.WaitForBinaryFencesTimed(Span<IBinaryCPUWaitableFence *>(&fence, 1).ToValueISpan(), false, timeoutMSec);
	}

	Result VulkanBinaryCPUWaitableFence::ResetFence()
	{
		IBinaryCPUWaitableFence *fence = this;
		return m_device.ResetBinaryFences(Span<IBinaryCPUWaitableFence *>(&fence, 1).ToValueISpan());
	}

	VulkanBinaryGPUWaitableFence::VulkanBinaryGPUWaitableFence(VulkanDeviceBase &device)
		: m_device(device)
		, m_sema(VK_NULL_HANDLE)
	{
	}

	VulkanBinaryGPUWaitableFence::~VulkanBinaryGPUWaitableFence()
	{
		if (m_sema != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroySemaphore(m_device.GetDevice(), m_sema, m_device.GetAllocCallbacks());
	}

	Result VulkanBinaryGPUWaitableFence::Initialize()
	{
		VkSemaphoreCreateInfo semaCreateInfo = {};
		semaCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateSemaphore(m_device.GetDevice(), &semaCreateInfo, m_device.GetAllocCallbacks(), &m_sema));
		return ResultCode::kOK;
	}
}
