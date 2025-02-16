#include "VulkanRenderPassInstance.h"

#include "VulkanAPI.h"
#include "VulkanDevice.h"
#include "VulkanRenderPass.h"

#include "rkit/Render/PipelineLibraryItem.h"
#include "rkit/Render/RenderDefs.h"
#include "rkit/Render/RenderPassResources.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

namespace rkit::render::vulkan
{
	class VulkanRenderPassInstance final : public VulkanRenderPassInstanceBase
	{
	public:
		VulkanRenderPassInstance(VulkanDeviceBase &device);
		~VulkanRenderPassInstance();

		Result Initialize(const RenderPassRef_t &renderPassRef, const RenderPassResources &resources);

	private:
		VulkanDeviceBase &m_device;
		VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
	};

	VulkanRenderPassInstance::VulkanRenderPassInstance(VulkanDeviceBase &device)
		: m_device(device)
	{
	}

	VulkanRenderPassInstance::~VulkanRenderPassInstance()
	{
		if (m_framebuffer != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyFramebuffer(m_device.GetDevice(), m_framebuffer, m_device.GetAllocCallbacks());
	}

	Result VulkanRenderPassInstance::Initialize(const RenderPassRef_t &renderPassRef, const RenderPassResources &resources)
	{
		if (!renderPassRef.IsValid())
			return ResultCode::kInvalidParameter;

		if (resources.m_width == 0 || resources.m_height == 0)
			return ResultCode::kInvalidParameter;

		const RenderPassDesc *desc = renderPassRef.ResolveDesc();

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = static_cast<const VulkanRenderPassBase *>(renderPassRef.ResolveCompiled())->GetRenderPass();

		createInfo.attachmentCount = static_cast<uint32_t>(desc->m_renderTargets.Count());

		if (desc->m_depthStencilTarget != nullptr)
			createInfo.attachmentCount++;

		StaticArray<VkImageView, 9> staticAttachmentsStorage;
		Vector<VkImageView> dynamicAttachmentsStorage;

		Span<VkImageView> attachments;
		if (createInfo.attachmentCount <= staticAttachmentsStorage.Count())
			attachments = staticAttachmentsStorage.ToSpan().SubSpan(0, createInfo.attachmentCount);
		else
		{
			RKIT_CHECK(dynamicAttachmentsStorage.Resize(createInfo.attachmentCount));
			attachments = dynamicAttachmentsStorage.ToSpan();
		}

		if (createInfo.attachmentCount > 0)
			createInfo.pAttachments = attachments.Ptr();

		// TODO: Fill attachment list

		createInfo.width = resources.m_width;
		createInfo.height = resources.m_height;
		createInfo.layers = resources.m_arraySize;

		return ResultCode::kNotYetImplemented;
	}

	Result VulkanRenderPassInstanceBase::Create(UniquePtr<VulkanRenderPassInstanceBase> &renderPassInstance, VulkanDeviceBase &device, const RenderPassRef_t &renderPassRef, const RenderPassResources &resources)
	{
		UniquePtr<VulkanRenderPassInstance> instance;
		RKIT_CHECK(New<VulkanRenderPassInstance>(instance, device));

		renderPassInstance = std::move(instance);

		return ResultCode::kOK;
	}
}
