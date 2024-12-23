#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

#include "VulkanAPI.h"
#include "VulkanAPILoader.h"
#include "VulkanDevice.h"
#include "VulkanQueueProxy.h"


namespace rkit::render::vulkan
{
	class VulkanDevice final : public VulkanDeviceBase
	{
	public:
		VulkanDevice(const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, VkInstance inst, VkDevice device, const VkAllocationCallbacks *allocCallbacks, UniquePtr<IMutex> &&queueMutex);

		ICopyCommandQueue *GetCopyQueue(size_t index) const override;
		IComputeCommandQueue *GetComputeQueue(size_t index) const override;
		IGraphicsCommandQueue *GetGraphicsQueue(size_t index) const override;
		IGraphicsComputeCommandQueue *GetGraphicsComputeQueue(size_t index) const override;

		VkDevice GetDevice() const override;
		const VkAllocationCallbacks *GetAllocCallbacks() const override;

		Result CreateCPUWaitableFence(UniquePtr<ICPUWaitableFence> &outFence) override;

		Result LoadDeviceAPI();
		Result ResolveQueues(CommandQueueType queueType, size_t firstQueueID, uint32_t queueFamily, uint32_t numQueues);

		IMutex &GetQueueMutex();

	private:
		class FunctionResolver final : public IFunctionResolver
		{
		public:
			explicit FunctionResolver(const VulkanInstanceAPI &vki, VkDevice device);

			bool ResolveProc(void *pfnAddr, const FunctionLoaderInfo &fli, const StringView &name) const override;
			bool IsExtensionEnabled(const StringView &ext) const override;

		private:
			const VulkanInstanceAPI &m_vki;
			VkDevice m_device;
		};

		static const size_t kNumQueues = static_cast<size_t>(CommandQueueType::kCount);

		const VulkanGlobalAPI &m_vkg;
		const VulkanInstanceAPI &m_vki;
		VulkanDeviceAPI m_vkd;
		VkInstance m_inst;
		VkDevice m_device;
		const VkAllocationCallbacks *m_allocCallbacks;

		Vector<UniquePtr<QueueProxy>> m_queueFamilies[kNumQueues];
		Vector<QueueProxy *> m_allQueues;

		UniquePtr<IMutex> m_queueMutex;
	};

	VulkanDevice::FunctionResolver::FunctionResolver(const VulkanInstanceAPI &vki, VkDevice device)
		: m_device(device)
		, m_vki(vki)
	{
	}

	bool VulkanDevice::FunctionResolver::ResolveProc(void *pfnAddr, const FunctionLoaderInfo &fli, const StringView &name) const
	{
		PFN_vkVoidFunction func = m_vki.vkGetDeviceProcAddr(m_device, name.GetChars());
		if (func == nullptr)
			return false;
		else
		{
			fli.m_copyVoidFunctionCallback(pfnAddr, func);
			return true;
		}
	}

	bool VulkanDevice::FunctionResolver::IsExtensionEnabled(const StringView &ext) const
	{
		return false;
	}

	VulkanDevice::VulkanDevice(const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, VkInstance inst, VkDevice device, const VkAllocationCallbacks *allocCallbacks, UniquePtr<IMutex> &&queueMutex)
		: m_vkg(vkg)
		, m_vki(vki)
		, m_inst(inst)
		, m_device(device)
		, m_allocCallbacks(allocCallbacks)
		, m_queueMutex(std::move(queueMutex))
	{
	}

	Result VulkanDevice::ResolveQueues(CommandQueueType queueType, size_t firstQueueID, uint32_t queueFamily, uint32_t numQueues)
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		Vector<UniquePtr<QueueProxy>> &proxies = m_queueFamilies[static_cast<size_t>(queueType)];

		RKIT_CHECK(proxies.Resize(numQueues));

		for (uint32_t i = 0; i < numQueues; i++)
		{
			VkQueue queue = VK_NULL_HANDLE;
			m_vkd.vkGetDeviceQueue(m_device, queueFamily, i, &queue);

			RKIT_CHECK(New<QueueProxy>(proxies[i], alloc, *this, queue, m_vkd, firstQueueID + i));
		}

		return ResultCode::kOK;
	}

	ICopyCommandQueue *VulkanDevice::GetCopyQueue(size_t index) const
	{
		return m_queueFamilies[static_cast<size_t>(CommandQueueType::kCopy)][index].Get();
	}

	IComputeCommandQueue *VulkanDevice::GetComputeQueue(size_t index) const
	{
		return m_queueFamilies[static_cast<size_t>(CommandQueueType::kAsyncCompute)][index].Get();
	}

	IGraphicsCommandQueue *VulkanDevice::GetGraphicsQueue(size_t index) const
	{
		return m_queueFamilies[static_cast<size_t>(CommandQueueType::kGraphics)][index].Get();
	}

	IGraphicsComputeCommandQueue *VulkanDevice::GetGraphicsComputeQueue(size_t index) const
	{
		return m_queueFamilies[static_cast<size_t>(CommandQueueType::kGraphicsCompute)][index].Get();
	}

	VkDevice VulkanDevice::GetDevice() const
	{
		return m_device;
	}

	const VkAllocationCallbacks *VulkanDevice::GetAllocCallbacks() const
	{
		return m_allocCallbacks;
	}

	Result VulkanDevice::CreateCPUWaitableFence(UniquePtr<ICPUWaitableFence> &outFence)
	{
		return ResultCode::kOK;
	}

	Result VulkanDevice::LoadDeviceAPI()
	{
		ResolveFunctionCallback_t resolveCB = m_vkd.GetFirstResolveFunctionCallback();

		FunctionLoaderInfo functionLoaderInfo;

		RKIT_CHECK(LoadVulkanAPI(m_vkd, FunctionResolver(m_vki, m_device)));

		return ResultCode::kOK;
	}

	Result VulkanDeviceBase::CreateDevice(UniquePtr<IRenderDevice> &outDevice, const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, VkInstance inst, VkDevice device, const QueueFamilySpec(&queues)[static_cast<size_t>(CommandQueueType::kCount)], const VkAllocationCallbacks *allocCallbacks)
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;

		UniquePtr<IMutex> queueMutex;
		RKIT_CHECK(sysDriver->CreateMutex(queueMutex));

		UniquePtr<VulkanDevice> vkDevice;
		RKIT_CHECK(New<VulkanDevice>(vkDevice, vkg, vki, inst, device, allocCallbacks, std::move(queueMutex)));

		RKIT_CHECK(vkDevice->LoadDeviceAPI());

		size_t firstQueueID = 0;
		for (size_t i = 0; i < static_cast<size_t>(CommandQueueType::kCount); i++)
		{
			CommandQueueType queueType = static_cast<CommandQueueType>(i);
			const QueueFamilySpec &spec = queues[i];

			if (spec.m_numQueues > 0)
			{
				RKIT_CHECK(vkDevice->ResolveQueues(queueType, firstQueueID, spec.m_queueFamily, spec.m_numQueues));

				firstQueueID += spec.m_numQueues;
			}
		}

		outDevice = std::move(vkDevice);

		return ResultCode::kOK;
	}
}
