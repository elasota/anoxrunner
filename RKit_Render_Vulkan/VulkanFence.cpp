#include "VulkanFence.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"

namespace rkit { namespace render { namespace vulkan
{
	VulkanTimelineFence::VulkanTimelineFence(VulkanDeviceBase &device)
		: m_device(device)
		, m_sema(VK_NULL_HANDLE)
	{
	}

	VulkanTimelineFence::~VulkanTimelineFence()
	{
		if (m_sema != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroySemaphore(m_device.GetDevice(), m_sema, m_device.GetAllocCallbacks());
	}

	Result VulkanTimelineFence::Initialize(uint64_t initialValue)
	{
		VkSemaphoreTypeCreateInfoKHR typeCreateInfo = {};
		typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
		typeCreateInfo.initialValue = initialValue;
		typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;

		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		createInfo.pNext = &typeCreateInfo;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateSemaphore(m_device.GetDevice(), &createInfo, m_device.GetAllocCallbacks(), &m_sema));

		RKIT_RETURN_OK;
	}

	Result VulkanTimelineFence::SetValue(TimelinePoint_t value)
	{
		VkSemaphoreSignalInfoKHR signalInfo = {};
		signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR;
		signalInfo.semaphore = m_sema;
		signalInfo.value = value;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkSignalSemaphoreKHR(m_device.GetDevice(), &signalInfo));

		RKIT_RETURN_OK;
	}

	Result VulkanTimelineFence::GetCurrentValue(TimelinePoint_t &outValue) const
	{
		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkGetSemaphoreCounterValueKHR(m_device.GetDevice(), m_sema, &outValue));

		RKIT_RETURN_OK;
	}

	VkSemaphore VulkanTimelineFence::GetSemaphore() const
	{
		return m_sema;
	}

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
		RKIT_RETURN_OK;
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
		RKIT_RETURN_OK;
	}
} } } // rkit::render::vulkan
