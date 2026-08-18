#include "VulkanQueueProxy.h"

#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

#include "VulkanAPI.h"
#include "VulkanCheck.h"
#include "VulkanDevice.h"
#include "VulkanCommandAllocator.h"
#include "VulkanFence.h"
#include "VulkanUtils.h"

#include <cstring>

namespace rkit::render::vulkan
{
	class VulkanQueueProxy final : public VulkanQueueProxyBase
	{
	public:
		VulkanQueueProxy(IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI);
		~VulkanQueueProxy();

		Result CreateCopyCommandAllocator(UniquePtr<ICopyCommandAllocator> &outCommandAllocator, bool isBundle) override;
		Result CreateComputeCommandAllocator(UniquePtr<IComputeCommandAllocator> &outCommandAllocator, bool isBundle) override;
		Result CreateGraphicsCommandAllocator(UniquePtr<IGraphicsCommandAllocator> &outCommandAllocator, bool isBundle) override;
		Result CreateGraphicsComputeCommandAllocator(UniquePtr<IGraphicsComputeCommandAllocator> &outCommandAllocator, bool isBundle) override;

		CommandQueueType GetCommandQueueType() const override;

		ICopyCommandQueue *ToCopyCommandQueue() override;
		IComputeCommandQueue *ToComputeCommandQueue() override;
		IGraphicsCommandQueue *ToGraphicsCommandQueue() override;
		IGraphicsComputeCommandQueue *ToGraphicsComputeCommandQueue() override;
		IInternalCommandQueue *ToInternalCommandQueue() override;

		uint32_t GetQueueFamily() const override;

		VkQueue GetVkQueue() const override;

		Result Initialize();

		template<class T>
		Result CreateTypedCommandAllocator(UniquePtr<T> &outCommandAllocator, bool isBundle);

		Result CreateCommandAllocator(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, bool isBundle);

	protected:
		DynamicCastRef_t InternalDynamicCast() override;

	private:
		IMallocDriver *m_alloc;
		CommandQueueType m_queueType;
		VulkanDeviceBase &m_device;
		VkQueue m_queue;
		uint32_t m_queueFamily;
		const VulkanDeviceAPI &m_vkd;
	};
}

namespace rkit
{
	template<>
	struct DynamicDowncaster<render::vulkan::VulkanQueueProxy, render::ICopyCommandQueue>
	{
		static render::ICopyCommandQueue *Cast(render::vulkan::VulkanQueueProxy *src);
	};
}

namespace rkit::render::vulkan
{
	VulkanQueueProxy::VulkanQueueProxy(IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI)
		: m_alloc(alloc)
		, m_queueType(queueType)
		, m_device(device)
		, m_queue(queue)
		, m_queueFamily(queueFamily)
		, m_vkd(deviceAPI)
	{
	}

	VulkanQueueProxy::~VulkanQueueProxy()
	{
	}

	Result VulkanQueueProxy::CreateCopyCommandAllocator(UniquePtr<ICopyCommandAllocator> &outCommandAllocator, bool isBundle)
	{
		UniquePtr<IComputeCommandAllocator> computeAlloc;
		CreateTypedCommandAllocator(computeAlloc, isBundle);

		outCommandAllocator = std::move(computeAlloc);
		RKIT_RETURN_OK;
	}

	Result VulkanQueueProxy::CreateComputeCommandAllocator(UniquePtr<IComputeCommandAllocator> &outCommandAllocator, bool isBundle)
	{
		return CreateTypedCommandAllocator(outCommandAllocator, isBundle);
	}

	Result VulkanQueueProxy::CreateGraphicsCommandAllocator(UniquePtr<IGraphicsCommandAllocator> &outCommandAllocator, bool isBundle)
	{
		return CreateTypedCommandAllocator(outCommandAllocator, isBundle);
	}

	Result VulkanQueueProxy::CreateGraphicsComputeCommandAllocator(UniquePtr<IGraphicsComputeCommandAllocator> &outCommandAllocator, bool isBundle)
	{
		return CreateTypedCommandAllocator(outCommandAllocator, isBundle);
	}

	CommandQueueType VulkanQueueProxy::GetCommandQueueType() const
	{
		return m_queueType;
	}

	ICopyCommandQueue *VulkanQueueProxy::ToCopyCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kCopy))
		{
			IComputeCommandQueue *downcast = this;
			return downcast;
		}
		else
			return nullptr;
	}

	IComputeCommandQueue *VulkanQueueProxy::ToComputeCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kAsyncCompute))
			return this;
		else
			return nullptr;
	}

	IGraphicsCommandQueue *VulkanQueueProxy::ToGraphicsCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kGraphics))
			return this;
		else
			return nullptr;
	}

	IGraphicsComputeCommandQueue *VulkanQueueProxy::ToGraphicsComputeCommandQueue()
	{
		if (IsQueueTypeCompatible(m_queueType, CommandQueueType::kGraphicsCompute))
			return this;
		else
			return nullptr;
	}

	IInternalCommandQueue *VulkanQueueProxy::ToInternalCommandQueue()
	{
		return this;
	}

	VkQueue VulkanQueueProxy::GetVkQueue() const
	{
		return m_queue;
	}

	uint32_t VulkanQueueProxy::GetQueueFamily() const
	{
		return m_queueFamily;
	}

	Result VulkanQueueProxy::Initialize()
	{
		RKIT_RETURN_OK;
	}

	template<class T>
	Result VulkanQueueProxy::CreateTypedCommandAllocator(UniquePtr<T> &outCommandAllocator, bool isBundle)
	{
		UniquePtr<VulkanCommandAllocatorBase> cmdAllocator;
		CreateCommandAllocator(cmdAllocator, isBundle);

		outCommandAllocator = std::move(cmdAllocator);

		RKIT_RETURN_OK;
	}

	Result VulkanQueueProxy::CreateCommandAllocator(UniquePtr<VulkanCommandAllocatorBase> &outCommandAllocator, bool isBundle)
	{
		return VulkanCommandAllocatorBase::Create(outCommandAllocator, m_device, *this, m_queueType, isBundle, m_queueFamily);
	}

	VulkanQueueProxy::DynamicCastRef_t VulkanQueueProxy::InternalDynamicCast()
	{
		switch (m_queueType)
		{
		case CommandQueueType::kGraphics:
			return DynamicCastRef_t::CreateFrom<VulkanQueueProxy, IGraphicsCommandQueue, ICopyCommandQueue>(this);
		case CommandQueueType::kGraphicsCompute:
			return DynamicCastRef_t::CreateFrom<VulkanQueueProxy, IGraphicsCommandQueue, IComputeCommandQueue, IGraphicsComputeCommandQueue, ICopyCommandQueue>(this);
		case CommandQueueType::kAsyncCompute:
			return DynamicCastRef_t::CreateFrom<VulkanQueueProxy, IComputeCommandQueue, ICopyCommandQueue>(this);
		case CommandQueueType::kCopy:
			return DynamicCastRef_t::CreateFrom<VulkanQueueProxy, ICopyCommandQueue>(this);
		default:
			return DynamicCastRef_t();
		}
	}

	Result VulkanQueueProxyBase::Create(UniquePtr<VulkanQueueProxyBase> &outQueueProxy, IMallocDriver *alloc, CommandQueueType queueType, VulkanDeviceBase &device, VkQueue queue, uint32_t queueFamily, const VulkanDeviceAPI &deviceAPI)
	{
		UniquePtr<VulkanQueueProxy> queueProxy = New<VulkanQueueProxy>(alloc, queueType, device, queue, queueFamily, deviceAPI);

		queueProxy->Initialize();

		outQueueProxy = std::move(queueProxy);
	}
} // rkit::render::vulkan


namespace rkit
{
	render::ICopyCommandQueue *DynamicDowncaster<render::vulkan::VulkanQueueProxy, render::ICopyCommandQueue>::Cast(render::vulkan::VulkanQueueProxy *src)
	{
		return DynamicDowncaster<render::vulkan::VulkanQueueProxyBase, render::ICopyCommandQueue>::Cast(src);
	}
}
