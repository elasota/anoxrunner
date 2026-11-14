#include "VulkanResourceTable.h"

#include "IncludeVulkan.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/Vector.h"

namespace rkit::render::vulkan
{
	class HeterogeneousResourceTableImpl final : public OpaqueImplementation<HeterogeneousResourceTable>
	{
	public:
		HeterogeneousResourceTableImpl(VulkanDeviceBase &device);
		~HeterogeneousResourceTableImpl();

		void Initialize(const ResourceTableSpec &spec);

	private:
		static constexpr size_t kNumResourceDescriptorSets = static_cast<size_t>(HeterogeneousResourceTable::DescriptorSetType::Count);

		struct DescriptorSetMetadata
		{
			VkDescriptorType m_descType = {};
			uint32_t m_descriptorSetIndex = 0;
			uint32_t m_numDescriptors = 0;
		};

		struct DescriptorSetAndLayout
		{
			VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
		};

		VulkanDeviceBase &m_device;

		VkDescriptorPool m_resourceDescriptorPool = VK_NULL_HANDLE;

		StaticArray<Optional<DescriptorSetMetadata>, kNumResourceDescriptorSets> m_resourceDescriptorSetMetadata;
	};

	HeterogeneousResourceTableImpl::HeterogeneousResourceTableImpl(VulkanDeviceBase &device)
		: m_device(device)
	{
	}

	HeterogeneousResourceTableImpl::~HeterogeneousResourceTableImpl()
	{
		VkDevice device = m_device.GetDevice();
		const VkAllocationCallbacks *allocCB = m_device.GetAllocCallbacks();
		const VulkanDeviceAPI &deviceAPI = m_device.GetDeviceAPI();

		if (m_resourceDescriptorPool != VK_NULL_HANDLE)
			deviceAPI.vkDestroyDescriptorPool(device, m_resourceDescriptorPool, allocCB);
	}

	void HeterogeneousResourceTableImpl::Initialize(const ResourceTableSpec &spec)
	{
		uint32_t numDescriptorSets = 0;
		auto applyDescriptorType = [this, &numDescriptorSets](HeterogeneousResourceTable::DescriptorSetType descriptorSetType, VkDescriptorType descType, uint32_t numDescriptors)
			{
				if (numDescriptors == 0)
					return;

				const size_t descriptorSetIndex = static_cast<size_t>(descriptorSetType);
				m_resourceDescriptorSetMetadata[descriptorSetIndex] = DescriptorSetMetadata();

				DescriptorSetMetadata &metadata = m_resourceDescriptorSetMetadata[descriptorSetIndex].Get();

				metadata.m_descType = descType;
				metadata.m_descriptorSetIndex = numDescriptorSets;
				metadata.m_numDescriptors = numDescriptors;

				numDescriptorSets++;
			};

		applyDescriptorType(HeterogeneousResourceTable::DescriptorSetType::Texture, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, spec.m_numTextureResources);
		applyDescriptorType(HeterogeneousResourceTable::DescriptorSetType::RWTexture, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, spec.m_numRWTextureResources);
		applyDescriptorType(HeterogeneousResourceTable::DescriptorSetType::Buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, spec.m_numBufferResources);
		applyDescriptorType(HeterogeneousResourceTable::DescriptorSetType::Sampler, VK_DESCRIPTOR_TYPE_SAMPLER, spec.m_numSamplerResources);

		StaticArray<VkDescriptorPoolSize, kNumResourceDescriptorSets> sizes;

		for (const Optional<DescriptorSetMetadata> &optMetadata : m_resourceDescriptorSetMetadata)
		{
			if (!optMetadata.IsSet())
				continue;

			const DescriptorSetMetadata &metadata = optMetadata.Get();

			VkDescriptorPoolSize &poolSize = sizes[metadata.m_descriptorSetIndex];
			poolSize.type = metadata.m_descType;
			poolSize.descriptorCount = metadata.m_numDescriptors;
		}

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		descriptorPoolCreateInfo.maxSets = numDescriptorSets;
		descriptorPoolCreateInfo.pPoolSizes = sizes.GetBuffer();
		descriptorPoolCreateInfo.poolSizeCount = numDescriptorSets;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateDescriptorPool(m_device.GetDevice(), &descriptorPoolCreateInfo, m_device.GetAllocCallbacks(), &m_resourceDescriptorPool));
	}

	HeterogeneousResourceTable::HeterogeneousResourceTable(VulkanDeviceBase &device)
		: Opaque<HeterogeneousResourceTableImpl>(device)
	{
	}

	UniquePtr<HeterogeneousResourceTable> HeterogeneousResourceTable::Create(VulkanDeviceBase &device, const ResourceTableSpec &spec)
	{
		UniquePtr<HeterogeneousResourceTable> table = New<HeterogeneousResourceTable>(device);

		table->Impl().Initialize(spec);

		return table;
	}
} // rkit::render::vulkan

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(rkit::render::vulkan::HeterogeneousResourceTableImpl)
