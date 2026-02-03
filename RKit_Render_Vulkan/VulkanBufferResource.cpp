#include "VulkanBufferResource.h"
#include "VulkanMemoryRequirements.h"

#include "VulkanAPI.h"
#include "VulkanQueueProxy.h"
#include "VulkanDevice.h"
#include "VulkanCheck.h"

#include "rkit/Render/CommandQueue.h"
#include "rkit/Render/BufferSpec.h"
#include "rkit/Render/Memory.h"

#include "rkit/Core/Span.h"
#include "rkit/Core/Vector.h"

namespace rkit { namespace render { namespace vulkan {

	VulkanBuffer::VulkanBuffer(VulkanDeviceBase &device, VkBuffer buffer, const MemoryAddress &baseAddress, GPUMemorySize_t size)
		: m_device(device)
		, m_buffer(buffer)
		, m_baseAddress(baseAddress)
		, m_size(size)
	{
	}

	VulkanBuffer::~VulkanBuffer()
	{
		if (m_buffer != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyBuffer(m_device.GetDevice(), m_buffer, m_device.GetAllocCallbacks());
	}

	VulkanBufferPrototype::VulkanBufferPrototype(VulkanDeviceBase &device, VkBuffer buffer)
		: m_device(device)
		, m_buffer(buffer)
		, m_memRequirements { &device }
	{
		m_device.GetDeviceAPI().vkGetBufferMemoryRequirements(m_device.GetDevice(), buffer, &m_memRequirements.m_memReqs);
	}

	VulkanBufferPrototype::~VulkanBufferPrototype()
	{
		if (m_buffer != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyBuffer(m_device.GetDevice(), m_buffer, m_device.GetAllocCallbacks());
	}

	MemoryRequirementsView VulkanBufferPrototype::GetMemoryRequirements() const
	{
		return ExportMemoryRequirements(m_memRequirements);
	}

	Result VulkanBufferPrototype::Create(UniquePtr<VulkanBufferPrototype> &outBufferPrototype, VulkanDeviceBase &device,
		const BufferSpec &bufferSpec, const BufferResourceSpec &resourceSpec, const ConstSpan<IBaseCommandQueue *> &concurrentQueues)
	{
		VkBufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

		for (BufferUsageFlag usageFlag : resourceSpec.m_usage)
		{
			switch (usageFlag)
			{
			case BufferUsageFlag::kCopySrc:
				createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				break;
			case BufferUsageFlag::kCopyDest:
				createInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				break;
			case BufferUsageFlag::kConstantBuffer:
				createInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				break;
			case BufferUsageFlag::kStorageBuffer:
				createInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
				break;
			case BufferUsageFlag::kIndexBuffer:
				createInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				break;
			case BufferUsageFlag::kVertexBuffer:
				createInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				break;
			case BufferUsageFlag::kIndirectBuffer:
				createInfo.usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
				break;
			default:
				break;
			};
		}

		if (bufferSpec.m_size > std::numeric_limits<VkDeviceSize>::max())
			RKIT_THROW(ResultCode::kOutOfMemory);

		createInfo.size = static_cast<VkDeviceSize>(bufferSpec.m_size);

		Vector<uint32_t> restrictedQueueFamilies;

		if (concurrentQueues.Count() == 0)
			createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		else
		{
			createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

			for (IBaseCommandQueue *queue : concurrentQueues)
			{
				VulkanQueueProxyBase *internalQueue = static_cast<VulkanQueueProxyBase *>(queue->ToInternalCommandQueue());

				const uint32_t queueFamily = internalQueue->GetQueueFamily();

				bool isNewQueue = true;
				for (uint32_t existingQueueFamily : restrictedQueueFamilies)
				{
					if (existingQueueFamily == queueFamily)
					{
						isNewQueue = false;
						break;
					}
				}

				if (isNewQueue)
				{
					RKIT_CHECK(restrictedQueueFamilies.Append(queueFamily));
				}
			}

			createInfo.pQueueFamilyIndices = restrictedQueueFamilies.GetBuffer();
			createInfo.queueFamilyIndexCount = static_cast<uint32_t>(restrictedQueueFamilies.Count());
		}

		VkBuffer buffer = VK_NULL_HANDLE;
		RKIT_VK_CHECK(device.GetDeviceAPI().vkCreateBuffer(device.GetDevice(), &createInfo, device.GetAllocCallbacks(), &buffer));

		RKIT_TRY_CATCH_RETHROW(New<VulkanBufferPrototype>(outBufferPrototype, device, buffer),
			CatchContext(
				[&device, buffer]
				{
					device.GetDeviceAPI().vkDestroyBuffer(device.GetDevice(), buffer, device.GetAllocCallbacks());
				}
			)
		);

		RKIT_RETURN_OK;
	}
} } }
