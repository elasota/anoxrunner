#pragma once

#include "rkit/Render/RenderPassInstance.h"
#include "rkit/Render/PipelineLibraryItemProtos.h"

#include "IncludeVulkan.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct RenderPassResources;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class VulkanRenderPassInstanceBase : public IRenderPassInstance
	{
	public:
		static Result Create(UniquePtr<VulkanRenderPassInstanceBase> &renderPassInstance, VulkanDeviceBase &device, const RenderPassRef_t &renderPassRef, const RenderPassResources &resources);
	};
}
