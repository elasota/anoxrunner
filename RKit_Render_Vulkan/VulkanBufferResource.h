#pragma once

#include "rkit/Core/SpanProtos.h"
#include "rkit/Render/BufferResource.h"

#include "VulkanMemoryRequirements.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit { namespace render {
	struct BufferSpec;
	struct BufferResourceSpec;
	struct IBaseCommandQueue;
} }

namespace rkit { namespace render { namespace vulkan {
	class VulkanDeviceBase;

	class VulkanBuffer final : public IBufferResource
	{
	public:
		VulkanBuffer(VulkanDeviceBase &device, VkBuffer buffer);
		~VulkanBuffer();

		VkBuffer GetVkBuffer() const;

	private:
		VulkanDeviceBase &m_device;
		VkBuffer m_buffer;
	};

	class VulkanBufferPrototype final : public IBufferPrototype
	{
	public:
		explicit VulkanBufferPrototype(VulkanDeviceBase &device, VkBuffer buffer);
		~VulkanBufferPrototype();

		MemoryRequirementsView GetMemoryRequirements() const override;

		VkBuffer GetBuffer() const;

		void DetachBuffer();

		static Result Create(UniquePtr<VulkanBufferPrototype> &outImagePrototype, VulkanDeviceBase &device,
			const BufferSpec &bufferSpec, const BufferResourceSpec &resourceSpec, const ConstSpan<IBaseCommandQueue *> &concurrentQueues);

	private:
		VulkanDeviceBase &m_device;
		VkBuffer m_buffer;
		VulkanDeviceMemoryRequirements m_memRequirements;
	};
} } }

namespace rkit { namespace render { namespace vulkan {
	inline VkBuffer VulkanBufferPrototype::GetBuffer() const
	{
		return m_buffer;
	}

	inline void VulkanBufferPrototype::DetachBuffer()
	{
		m_buffer = VK_NULL_HANDLE;
	}

	inline VkBuffer VulkanBuffer::GetVkBuffer() const
	{
		return m_buffer;
	}
} } }
