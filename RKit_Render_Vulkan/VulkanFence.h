#pragma once

#include "rkit/Render/Fence.h"

#include "IncludeVulkan.h"

namespace rkit { namespace render { namespace vulkan
{
	class VulkanDeviceBase;

	class VulkanTimelineFence final : public ICPUVisibleTimelineFence
	{
	public:
		VulkanTimelineFence(VulkanDeviceBase &device);
		~VulkanTimelineFence();

		Result Initialize(uint64_t initialValue);

		Result SetValue(TimelinePoint_t value) override;
		Result GetCurrentValue(TimelinePoint_t &outValue) const override;

		VkSemaphore GetSemaphore() const;

	private:
		VulkanDeviceBase &m_device;
		VkSemaphore m_sema;
	};

	class VulkanBinaryCPUWaitableFence final : public IBinaryCPUWaitableFence
	{
	public:
		explicit VulkanBinaryCPUWaitableFence(VulkanDeviceBase &device);
		~VulkanBinaryCPUWaitableFence();

		Result Initialize(bool startSignaled);

		Result WaitFor() override;
		Result WaitForTimed(uint64_t timeoutMSec) override;
		Result ResetFence() override;

		VkFence GetFence() const;

	private:
		VulkanDeviceBase &m_device;
		VkFence m_fence;
	};

	class VulkanBinaryGPUWaitableFence final : public IBinaryGPUWaitableFence
	{
	public:
		explicit VulkanBinaryGPUWaitableFence(VulkanDeviceBase &device);
		~VulkanBinaryGPUWaitableFence();

		Result Initialize();

		VkSemaphore GetSemaphore() const;

	private:
		VulkanDeviceBase &m_device;
		VkSemaphore m_sema;
	};
} } } // rkit::render::vulkan

namespace rkit { namespace render { namespace vulkan
{
	inline VkFence VulkanBinaryCPUWaitableFence::GetFence() const
	{
		return m_fence;
	}

	inline VkSemaphore VulkanBinaryGPUWaitableFence::GetSemaphore() const
	{
		return m_sema;
	}
} } } // rkit::render::vulkan
