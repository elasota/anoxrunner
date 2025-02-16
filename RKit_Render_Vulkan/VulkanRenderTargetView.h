#pragma once

#include "rkit/Render/RenderTargetView.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanRenderTargetViewBase : public IRenderTargetView
	{
	public:
		virtual VkImageView GetImageView() const = 0;

		static Result Create(UniquePtr<VulkanRenderTargetViewBase> &outRTV, VulkanDeviceBase &device);

	private:
	};
}
