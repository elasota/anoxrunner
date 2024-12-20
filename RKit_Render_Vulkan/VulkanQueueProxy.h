#pragma once

#include "rkit/Render/CommandQueue.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	struct VulkanDeviceAPI;

	class QueueProxy final : public IGraphicsComputeCommandQueue
	{
	public:
		void Init(VkQueue queue, const VulkanDeviceAPI &deviceAPI);

		Result ExecuteCopy(const Span<ICopyCommandList *> &cmdLists) override;
		Result ExecuteCompute(const Span<IComputeCommandList *> &cmdLists) override;
		Result ExecuteCompute(const Span<IGraphicsCommandList *> &cmdLists) override;
		Result ExecuteGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists) override;

	private:
		template<class T>
		Result ExecuteGeneric(const Span<T *> &cmdLists);

		template<class T, size_t TCapacity>
		Result ExecuteGenericSized(const Span<T *> &cmdLists);

		VkQueue m_queue = VK_NULL_HANDLE;
		const VulkanDeviceAPI *m_vkd = nullptr;
	};
}
