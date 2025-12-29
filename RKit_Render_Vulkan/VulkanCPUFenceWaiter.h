#pragma once

#include "rkit/Render/Fence.h"

#include "IncludeVulkan.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit { namespace render { namespace vulkan {

	class VulkanDeviceBase;

	class VulkanCPUFenceWaiterBase : public ICPUFenceWaiter
	{
	public:
		static Result Create(UniquePtr<VulkanCPUFenceWaiterBase> &outInstance, VulkanDeviceBase &device);
	};
} } }
