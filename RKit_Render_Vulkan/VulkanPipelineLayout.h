#pragma once

#include "rkit/Render/PipelineLayout.h"
#include "rkit/Vulkan/DescriptorHeap.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Optional.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{
	struct VulkanPipelineLayoutMetadata
	{
		static constexpr size_t kNumHeaps = static_cast<size_t>(DescriptorHeapType::Count);

		rkit::StaticArray<rkit::Optional<uint32_t>, kNumHeaps> m_descriptorHeapIndexes;
		rkit::Optional<uint32_t> m_constantBufferDescriptorSetIndex;
		uint32_t m_numPushConstants = 0;
		uint8_t m_numConstantBuffers = 0;
	};

	struct VulkanPipelineLayout : public IInternalPipelineLayout
	{
		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

		VulkanPipelineLayoutMetadata m_metadata;

		virtual uint8_t GetConstantBindingCount() const { return m_metadata.m_numConstantBuffers; }
	};
}

