#include "VulkanConstantBufferMultiView.h"

#include "rkit/Core/Vector.h"

#include "rkit/Render/ConstantBufferMultiView.h"

#include "VulkanBufferResource.h"
#include "VulkanDevice.h"
#include "VulkanAPI.h"
#include "VulkanCheck.h"

#include "IncludeVulkan.h"

namespace rkit::render::vulkan
{

	class VulkanConstantBufferMultiViewImpl final : public OpaqueImplementation<VulkanConstantBufferMultiView>
	{
	public:
		friend class VulkanConstantBufferMultiView;

		explicit VulkanConstantBufferMultiViewImpl(VulkanDeviceBase &device);
		~VulkanConstantBufferMultiViewImpl();

		void Initialize(const ISpan<const render::ConstantBufferMultiViewBufferSpec &> &buffers);

	private:
		struct DescriptorSetAndLayout
		{
			VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
			VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
		};

		VulkanDeviceBase &m_device;

		VkDescriptorPool m_constantDescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
	};

	VulkanConstantBufferMultiViewImpl::VulkanConstantBufferMultiViewImpl(VulkanDeviceBase &device)
		: m_device(device)
	{
	}

	VulkanConstantBufferMultiViewImpl::~VulkanConstantBufferMultiViewImpl()
	{
		VkDevice device = m_device.GetDevice();
		const VkAllocationCallbacks *allocCB = m_device.GetAllocCallbacks();
		const VulkanDeviceAPI &deviceAPI = m_device.GetDeviceAPI();

		if (m_descriptorSetLayout != VK_NULL_HANDLE)
			deviceAPI.vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, allocCB);

		if (m_constantDescriptorPool != VK_NULL_HANDLE)
			deviceAPI.vkDestroyDescriptorPool(device, m_constantDescriptorPool, allocCB);
	}

	void VulkanConstantBufferMultiViewImpl::Initialize(const ISpan<const render::ConstantBufferMultiViewBufferSpec &> &buffers)
	{
		const VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

		VkDescriptorPoolSize poolSize = {};
		poolSize.type = descriptorType;
		poolSize.descriptorCount = buffers.Count();

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		descriptorPoolCreateInfo.maxSets = 1;
		descriptorPoolCreateInfo.pPoolSizes = &poolSize;
		descriptorPoolCreateInfo.poolSizeCount = 1;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateDescriptorPool(m_device.GetDevice(), &descriptorPoolCreateInfo, m_device.GetAllocCallbacks(), &m_constantDescriptorPool));

		Vector<VkDescriptorSetLayoutBinding> layoutBindings;

		layoutBindings.Resize(buffers.Count());

		for (size_t bindingIndex = 0; bindingIndex < buffers.Count(); bindingIndex++)
		{
			VkDescriptorSetLayoutBinding &outBinding = layoutBindings[bindingIndex];
			const render::ConstantBufferMultiViewBufferSpec &inBinding = buffers[bindingIndex];

			outBinding = {};
			outBinding.binding = bindingIndex;
			outBinding.descriptorCount = 1;
			outBinding.descriptorType = descriptorType;
			outBinding.stageFlags = VK_SHADER_STAGE_ALL;
		}

		VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
		setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayoutCreateInfo.bindingCount = layoutBindings.Count();
		setLayoutCreateInfo.pBindings = layoutBindings.GetBuffer();

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateDescriptorSetLayout(m_device.GetDevice(), &setLayoutCreateInfo, m_device.GetAllocCallbacks(), &m_descriptorSetLayout));

		VkDescriptorSetAllocateInfo descAllocInfo = {};
		descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descAllocInfo.descriptorPool = m_constantDescriptorPool;
		descAllocInfo.descriptorSetCount = 1;
		descAllocInfo.pSetLayouts = &m_descriptorSetLayout;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkAllocateDescriptorSets(m_device.GetDevice(), &descAllocInfo, &m_descriptorSet));


		for (size_t bindingIndex = 0; bindingIndex < buffers.Count(); bindingIndex++)
		{
			const render::ConstantBufferMultiViewBufferSpec &inBinding = buffers[bindingIndex];

			const VulkanBuffer *vkBuffer = static_cast<const VulkanBuffer *>(inBinding.m_resource);

			VkDescriptorBufferInfo descBufferInfo = {};
			descBufferInfo.buffer = vkBuffer->GetVkBuffer();
			descBufferInfo.offset = 0;
			descBufferInfo.range = inBinding.m_size;

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = m_descriptorSet;
			write.descriptorCount = 1;
			write.descriptorType = descriptorType;
			write.pBufferInfo = &descBufferInfo;

			m_device.GetDeviceAPI().vkUpdateDescriptorSets(m_device.GetDevice(), 1, &write, 0, nullptr);
		}
	}

	VkDescriptorSet VulkanConstantBufferMultiView::GetDescriptorSet() const
	{
		return Impl().m_descriptorSet;
	}

	UniquePtr<VulkanConstantBufferMultiView> VulkanConstantBufferMultiView::Create(VulkanDeviceBase &device, const ConstantBufferMultiViewSpec &spec)
	{
		UniquePtr<VulkanConstantBufferMultiView> cbv = New<VulkanConstantBufferMultiView>(device);
		cbv->Impl().Initialize(*spec.m_buffers);
		return cbv;
	}

	VulkanConstantBufferMultiView::VulkanConstantBufferMultiView(VulkanDeviceBase &device)
		: Opaque<VulkanConstantBufferMultiViewImpl>(device)
	{
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(rkit::render::vulkan::VulkanConstantBufferMultiViewImpl)
