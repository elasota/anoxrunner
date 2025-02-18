#include "VulkanRenderPassInstance.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDepthStencilView.h"
#include "VulkanDevice.h"
#include "VulkanRenderPass.h"
#include "VulkanRenderTargetView.h"

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
		VkRenderPass m_renderPass = VK_NULL_HANDLE;
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

		m_renderPass = static_cast<const VulkanRenderPassBase *>(renderPassRef.ResolveCompiled())->GetRenderPass();

		const RenderPassDesc *desc = renderPassRef.ResolveDesc();

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = m_renderPass;

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

		if (desc->m_renderTargets.Count() != resources.m_renderTargetViews.Count())
			return ResultCode::kInvalidParameter;

		if ((desc->m_depthStencilTarget == nullptr) != (resources.m_depthStencilView == nullptr))
			return ResultCode::kInvalidParameter;

		size_t attachmentIndex = 0;
		for (const IRenderTargetView *rtv : resources.m_renderTargetViews)
			attachments[attachmentIndex++] = static_cast<const VulkanRenderTargetViewBase *>(rtv)->GetImageView();

		if (resources.m_depthStencilView)
			attachments[attachmentIndex++] = static_cast<const VulkanDepthStencilViewBase *>(resources.m_depthStencilView)->GetImageView();

		createInfo.width = resources.m_width;
		createInfo.height = resources.m_height;
		createInfo.layers = resources.m_arraySize;

		if (createInfo.attachmentCount > 0)
			createInfo.pAttachments = attachments.Ptr();

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateFramebuffer(m_device.GetDevice(), &createInfo, m_device.GetAllocCallbacks(), &m_framebuffer));

		return ResultCode::kOK;
	}

	Result VulkanRenderPassInstanceBase::Create(UniquePtr<VulkanRenderPassInstanceBase> &renderPassInstance, VulkanDeviceBase &device, const RenderPassRef_t &renderPassRef, const RenderPassResources &resources)
	{
		UniquePtr<VulkanRenderPassInstance> instance;
		RKIT_CHECK(New<VulkanRenderPassInstance>(instance, device));

		RKIT_CHECK(instance->Initialize(renderPassRef, resources));

		renderPassInstance = std::move(instance);

		return ResultCode::kOK;
	}
}
