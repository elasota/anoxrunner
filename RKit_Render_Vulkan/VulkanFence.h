#pragma once

#include "rkit/Render/Fence.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanBinaryCPUWaitableFence final : public IBinaryCPUWaitableFence
	{
	public:
		explicit VulkanBinaryCPUWaitableFence(VulkanDeviceBase &device);
		~VulkanBinaryCPUWaitableFence();

		Result Initialize(bool startSignaled);

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
}

namespace rkit::render::vulkan
{
	inline VkFence VulkanBinaryCPUWaitableFence::GetFence() const
	{
		return m_fence;
	}

	inline VkSemaphore VulkanBinaryGPUWaitableFence::GetSemaphore() const
	{
		return m_sema;
	}
}
