#include "VulkanDevice.h"

#include "VulkanAPI.h"
#include "VulkanAPILoader.h"
#include "VulkanCommandAllocator.h"
#include "VulkanCheck.h"
#include "VulkanFence.h"
#include "VulkanPipelineLibraryLoader.h"
#include "VulkanPlatformAPI.h"
#include "VulkanSwapChain.h"
#include "VulkanQueueProxy.h"
#include "VulkanPhysDevice.h"
#include "VulkanPlatformSpecific.h"
#include "VulkanResourcePool.h"

#include "rkit/Render/DeviceCaps.h"

#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/Vector.h"

namespace rkit::render::vulkan
{
	class VulkanDevice final : public VulkanDeviceBase
	{
	public:
		VulkanDevice(const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, const VulkanGlobalPlatformAPI &vkg_p, const VulkanInstancePlatformAPI &vki_p, VkInstance inst, VkDevice device, const VkAllocationCallbacks *allocCallbacks, const RenderDeviceCaps &caps, const RCPtr<RenderVulkanPhysicalDevice> &physDevice, Vector<StringView> &&enabledExts, UniquePtr<IMutex> &&queueMutex);
		~VulkanDevice();

		CallbackSpan<ICopyCommandQueue *, const void *> GetCopyQueues() const override;
		CallbackSpan<IComputeCommandQueue *, const void *> GetComputeQueues() const override;
		CallbackSpan<IGraphicsCommandQueue *, const void *> GetGraphicsQueues() const override;
		CallbackSpan<IGraphicsComputeCommandQueue *, const void *> GetGraphicsComputeQueues() const override;

		Result CreateBinaryCPUWaitableFence(UniquePtr<IBinaryCPUWaitableFence> &outFence, bool startSignaled) override;
		Result CreateBinaryGPUWaitableFence(UniquePtr<IBinaryGPUWaitableFence> &outFence) override;
		Result CreateSwapChainSyncPoint(UniquePtr<ISwapChainSyncPoint> &outSyncPoint) override;

		Result CreateRenderPassResources(UniquePtr<IRenderPassResources> &outResources, const RenderPassRef_t &renderPass, const RenderPassResourcesDesc &desc) override;

		Result ResetBinaryFences(const ISpan<IBinaryCPUWaitableFence *> &fences) override;
		Result WaitForBinaryFences(const ISpan<IBinaryCPUWaitableFence *> &fences, bool waitForAll) override;
		Result WaitForBinaryFencesTimed(const ISpan<IBinaryCPUWaitableFence *> &fences, bool waitForAll, uint64_t timeoutMSec) override;
		Result WaitForDeviceIdle() override;

		VkDevice GetDevice() const override;
		VkInstance GetInstance() const override;
		const VkAllocationCallbacks *GetAllocCallbacks() const override;

		const IRenderDeviceCaps &GetCaps() const override;

		Result CreatePipelineLibraryLoader(UniquePtr<IPipelineLibraryLoader> &loader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart) override;

		const VulkanGlobalAPI &GetGlobalAPI() const override;
		const VulkanInstanceAPI &GetInstanceAPI() const override;
		const VulkanDeviceAPI &GetDeviceAPI() const override;

		const VulkanGlobalPlatformAPI &GetGlobalPlatformAPI() const override;
		const VulkanInstancePlatformAPI &GetInstancePlatformAPI() const override;
		const VulkanDevicePlatformAPI &GetDevicePlatformAPI() const override;

		const RenderVulkanPhysicalDevice &GetPhysDevice() const override;

		IResourcePool<VkSemaphore> &GetBinarySemaPool() const override;
		IResourcePool<VkFence> &GetFencePool() const override;

		VulkanQueueProxyBase &GetQueueByID(size_t queueID) override;
		size_t GetQueueCount() const override;

		Result CreateSwapChainPrototype(UniquePtr<ISwapChainPrototype> &outSwapChainPrototype, IDisplay &display) override;

		Result CreateSwapChain(UniquePtr<ISwapChain> &outSwapChain, UniquePtr<ISwapChainPrototype> &&prototype, uint8_t numImages, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, IBaseCommandQueue &commandQueue) override;

		Result LoadDeviceAPI();
		Result CreatePools();
		Result ResolveQueues(CommandQueueType queueType, size_t firstQueueID, uint32_t queueFamily, uint32_t numQueues);
		Result FinalizeQueueList();

	private:
		class FunctionResolver final : public IFunctionResolver
		{
		public:
			explicit FunctionResolver(const VulkanInstanceAPI &vki, const VulkanDevice &device);

			bool ResolveProc(void *pfnAddr, const FunctionLoaderInfo &fli, const StringView &name) const override;
			bool IsExtensionEnabled(const StringView &ext) const override;

		private:
			const VulkanInstanceAPI &m_vki;
			const VulkanDevice &m_device;
		};

		class SemaFactory final : public IPooledResourceFactory<VkSemaphore>
		{
		public:
			explicit SemaFactory(VulkanDevice &device);

			void EmptyInitResource(VkSemaphore &outResource) const override;
			void DestructResource(VkSemaphore &resource) const override;

			Result InitResource(VkSemaphore &resource) const override;
			void RetireResource(VkSemaphore &resource) const override;
			Result UnretireResource(VkSemaphore &resource) const override;

		private:
			VulkanDevice &m_device;
		};

		class FenceFactory final : public IPooledResourceFactory<VkFence>
		{
		public:
			explicit FenceFactory(VulkanDevice &device);

			void EmptyInitResource(VkFence &outResource) const override;
			void DestructResource(VkFence &resource) const override;

			Result InitResource(VkFence &resource) const override;
			void RetireResource(VkFence &resource) const override;
			Result UnretireResource(VkFence &resource) const override;

		private:
			VulkanDevice &m_device;
		};

		struct QueueFamily
		{
			Vector<UniquePtr<VulkanQueueProxyBase>> m_queues;
			uint32_t m_vkQueueFamily = 0;
		};

		typedef const void *ConstVoidPtr_t;

		template<class T>
		static CallbackSpan<T *, const void *> CreateCallbackSpanForQueueFamily(const QueueFamily &queueFamily);

		template<class T>
		static T *QueueFamilySpanGetElement(const ConstVoidPtr_t &queueFamilyPtr, size_t index);

		static const size_t kNumQueues = static_cast<size_t>(CommandQueueType::kCount);

		const VulkanGlobalAPI &m_vkg;
		const VulkanInstanceAPI &m_vki;
		VulkanDeviceAPI m_vkd;

		const VulkanGlobalPlatformAPI &m_vkg_p;
		const VulkanInstancePlatformAPI &m_vki_p;
		VulkanDevicePlatformAPI m_vkd_p;

		VkInstance m_inst;
		VkDevice m_device;
		const VkAllocationCallbacks *m_allocCallbacks;
		RenderDeviceCaps m_caps;
		RCPtr<RenderVulkanPhysicalDevice> m_physDevice;

		StaticArray<QueueFamily, kNumQueues> m_queueFamilies;
		Vector<VulkanQueueProxyBase *> m_allQueues;

		Vector<StringView> m_deviceExtensions;

		UniquePtr<IMutex> m_queueMutex;

		SemaFactory m_semaFactory;
		UniquePtr<IResourcePool<VkSemaphore>> m_semaPool;

		FenceFactory m_fenceFactory;
		UniquePtr<IResourcePool<VkFence>> m_fencePool;
	};

	VulkanDevice::FunctionResolver::FunctionResolver(const VulkanInstanceAPI &vki, const VulkanDevice &device)
		: m_device(device)
		, m_vki(vki)
	{
	}

	bool VulkanDevice::FunctionResolver::ResolveProc(void *pfnAddr, const FunctionLoaderInfo &fli, const StringView &name) const
	{
		PFN_vkVoidFunction func = m_vki.vkGetDeviceProcAddr(m_device.GetDevice(), name.GetChars());
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
		for (const StringView &enabledExt : m_device.m_deviceExtensions)
		{
			if (enabledExt == ext)
				return true;
		}

		return false;
	}

	VulkanDevice::SemaFactory::SemaFactory(VulkanDevice &device)
		: m_device(device)
	{
	}

	void VulkanDevice::SemaFactory::EmptyInitResource(VkSemaphore &outResource) const
	{
		outResource = VK_NULL_HANDLE;
	}

	void VulkanDevice::SemaFactory::DestructResource(VkSemaphore &resource) const
	{
		if (resource != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroySemaphore(m_device.GetDevice(), resource, m_device.GetAllocCallbacks());
	}

	Result VulkanDevice::SemaFactory::InitResource(VkSemaphore &resource) const
	{
		VkSemaphoreCreateInfo semaCreateInfo = {};
		semaCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateSemaphore(m_device.GetDevice(), &semaCreateInfo, m_device.GetAllocCallbacks(), &resource));

		return ResultCode::kOK;
	}

	void VulkanDevice::SemaFactory::RetireResource(VkSemaphore &resource) const
	{
	}

	Result VulkanDevice::SemaFactory::UnretireResource(VkSemaphore &resource) const
	{
		return ResultCode::kOK;
	}

	VulkanDevice::FenceFactory::FenceFactory(VulkanDevice &device)
		: m_device(device)
	{
	}

	void VulkanDevice::FenceFactory::EmptyInitResource(VkFence &outResource) const
	{
		outResource = VK_NULL_HANDLE;
	}

	void VulkanDevice::FenceFactory::DestructResource(VkFence &resource) const
	{
		if (resource != VK_NULL_HANDLE)
			m_device.GetDeviceAPI().vkDestroyFence(m_device.GetDevice(), resource, m_device.GetAllocCallbacks());
	}

	Result VulkanDevice::FenceFactory::InitResource(VkFence &resource) const
	{
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		RKIT_VK_CHECK(m_device.GetDeviceAPI().vkCreateFence(m_device.GetDevice(), &fenceCreateInfo, m_device.GetAllocCallbacks(), &resource));

		return ResultCode::kOK;
	}

	void VulkanDevice::FenceFactory::RetireResource(VkFence &resource) const
	{
	}

	Result VulkanDevice::FenceFactory::UnretireResource(VkFence &resource) const
	{
		return ResultCode::kOK;
	}

	VulkanDevice::VulkanDevice(const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, const VulkanGlobalPlatformAPI &vkg_p, const VulkanInstancePlatformAPI &vki_p, VkInstance inst, VkDevice device, const VkAllocationCallbacks *allocCallbacks, const RenderDeviceCaps &caps, const RCPtr<RenderVulkanPhysicalDevice> &physDevice, Vector<StringView> &&enabledExts, UniquePtr<IMutex> &&queueMutex)
		: m_vkg(vkg)
		, m_vki(vki)
		, m_vkg_p(vkg_p)
		, m_vki_p(vki_p)
		, m_inst(inst)
		, m_device(device)
		, m_allocCallbacks(allocCallbacks)
		, m_deviceExtensions(std::move(enabledExts))
		, m_queueMutex(std::move(queueMutex))
		, m_caps(caps)
		, m_physDevice(physDevice)
		, m_semaFactory(*this)
		, m_fenceFactory(*this)
	{
	}

	VulkanDevice::~VulkanDevice()
	{
		m_vki.vkDestroyDevice(m_device, m_allocCallbacks);
	}

	Result VulkanDevice::ResolveQueues(CommandQueueType queueType, size_t firstQueueID, uint32_t queueFamilyIndex, uint32_t numQueues)
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		QueueFamily &queueFamily = m_queueFamilies[static_cast<size_t>(queueType)];
		queueFamily.m_vkQueueFamily = queueFamilyIndex;

		RKIT_CHECK(queueFamily.m_queues.Resize(numQueues));

		for (uint32_t i = 0; i < numQueues; i++)
		{
			VkQueue queue = VK_NULL_HANDLE;
			m_vkd.vkGetDeviceQueue(m_device, queueFamilyIndex, i, &queue);

			RKIT_CHECK(VulkanQueueProxyBase::Create(queueFamily.m_queues[i], alloc, queueType, *this, queue, queueFamilyIndex, m_vkd));
		}

		return ResultCode::kOK;
	}

	Result VulkanDevice::FinalizeQueueList()
	{
		size_t numQueues = 0;
		for (const QueueFamily &queueFamily : m_queueFamilies)
			numQueues += queueFamily.m_queues.Count();

		RKIT_CHECK(m_allQueues.Resize(numQueues));

		size_t insertIndex = 0;
		for (const QueueFamily &queueFamily : m_queueFamilies)
		{
			for (const UniquePtr<VulkanQueueProxyBase> &queuePtr : queueFamily.m_queues)
			{
				m_allQueues[insertIndex] = queuePtr.Get();
				insertIndex++;
			}
		}

		return ResultCode::kOK;
	}

	template<class T>
	CallbackSpan<T *, const void *> VulkanDevice::CreateCallbackSpanForQueueFamily(const QueueFamily &queueFamily)
	{
		return CallbackSpan<T *, const void *>(QueueFamilySpanGetElement<T>, &queueFamily, queueFamily.m_queues.Count());
	}

	template<class T>
	T *VulkanDevice::QueueFamilySpanGetElement(const ConstVoidPtr_t &queueFamilyPtr, size_t index)
	{
		const QueueFamily *queueFamily = static_cast<const QueueFamily *>(queueFamilyPtr);
		return queueFamily->m_queues[index].Get();
	}

	CallbackSpan<ICopyCommandQueue *, const void *> VulkanDevice::GetCopyQueues() const
	{
		return CreateCallbackSpanForQueueFamily<ICopyCommandQueue>(m_queueFamilies[static_cast<size_t>(CommandQueueType::kCopy)]);
	}

	CallbackSpan<IComputeCommandQueue *, const void *> VulkanDevice::GetComputeQueues() const
	{
		return CreateCallbackSpanForQueueFamily<IComputeCommandQueue>(m_queueFamilies[static_cast<size_t>(CommandQueueType::kAsyncCompute)]);
	}

	CallbackSpan<IGraphicsCommandQueue *, const void *> VulkanDevice::GetGraphicsQueues() const
	{
		return CreateCallbackSpanForQueueFamily<IGraphicsCommandQueue>(m_queueFamilies[static_cast<size_t>(CommandQueueType::kGraphics)]);
	}

	CallbackSpan<IGraphicsComputeCommandQueue *, const void *> VulkanDevice::GetGraphicsComputeQueues() const
	{
		return CreateCallbackSpanForQueueFamily<IGraphicsComputeCommandQueue>(m_queueFamilies[static_cast<size_t>(CommandQueueType::kGraphicsCompute)]);
	}

	VkDevice VulkanDevice::GetDevice() const
	{
		return m_device;
	}

	Result VulkanDevice::CreateBinaryCPUWaitableFence(UniquePtr<IBinaryCPUWaitableFence> &outFence, bool startSignaled)
	{
		UniquePtr<VulkanBinaryCPUWaitableFence> fence;
		RKIT_CHECK(New<VulkanBinaryCPUWaitableFence>(fence, *this));

		RKIT_CHECK(fence->Initialize(startSignaled));

		outFence = std::move(fence);

		return ResultCode::kOK;
	}

	Result VulkanDevice::CreateBinaryGPUWaitableFence(UniquePtr<IBinaryGPUWaitableFence> &outFence)
	{
		UniquePtr<VulkanBinaryGPUWaitableFence> fence;
		RKIT_CHECK(New<VulkanBinaryGPUWaitableFence>(fence, *this));

		RKIT_CHECK(fence->Initialize());

		outFence = std::move(fence);

		return ResultCode::kOK;
	}

	Result VulkanDevice::CreateSwapChainSyncPoint(UniquePtr<ISwapChainSyncPoint> &outSyncPoint)
	{
		UniquePtr<VulkanSwapChainSyncPointBase> syncPoint;
		RKIT_CHECK(VulkanSwapChainSyncPointBase::Create(syncPoint, *this));

		outSyncPoint = std::move(syncPoint);

		return ResultCode::kOK;
	}

	Result VulkanDevice::CreateRenderPassResources(UniquePtr<IRenderPassResources> &outResources, const RenderPassRef_t &renderPass, const RenderPassResourcesDesc &desc)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result VulkanDevice::ResetBinaryFences(const ISpan<IBinaryCPUWaitableFence *> &fences)
	{
		const size_t kMaxStaticFences = 16;

		size_t numFences = fences.Count();

		StaticArray<VkFence, kMaxStaticFences> staticFences;
		for (size_t firstFence = 0; firstFence < numFences; firstFence += kMaxStaticFences)
		{
			size_t fencesThisRound = numFences - firstFence;
			if (fencesThisRound > kMaxStaticFences)
				fencesThisRound = kMaxStaticFences;

			for (size_t i = 0; i < fencesThisRound; i++)
				staticFences[i] = static_cast<VulkanBinaryCPUWaitableFence *>(fences[i + firstFence])->GetFence();

			RKIT_VK_CHECK(m_vkd.vkResetFences(m_device, static_cast<uint32_t>(fencesThisRound), staticFences.GetBuffer()));
		}

		return ResultCode::kOK;
	}

	Result VulkanDevice::WaitForBinaryFences(const ISpan<IBinaryCPUWaitableFence *> &fences, bool waitForAll)
	{
		return WaitForBinaryFencesTimed(fences, std::numeric_limits<uint64_t>::max(), waitForAll);
	}

	Result VulkanDevice::WaitForBinaryFencesTimed(const ISpan<IBinaryCPUWaitableFence *> &fences, bool waitForAll, uint64_t timeoutMSec)
	{
		const size_t kMaxStaticFences = 16;

		size_t numFences = fences.Count();
		if (numFences <= kMaxStaticFences || (waitForAll && timeoutMSec == std::numeric_limits<uint64_t>::max()))
		{
			StaticArray<VkFence, kMaxStaticFences> staticFences;
			for (size_t firstFence = 0; firstFence < numFences; firstFence += kMaxStaticFences)
			{
				size_t fencesThisRound = numFences - firstFence;
				if (fencesThisRound > kMaxStaticFences)
					fencesThisRound = kMaxStaticFences;

				for (size_t i = 0; i < fencesThisRound; i++)
					staticFences[i] = static_cast<VulkanBinaryCPUWaitableFence *>(fences[i + firstFence])->GetFence();

				RKIT_VK_CHECK(m_vkd.vkWaitForFences(m_device, static_cast<uint32_t>(fencesThisRound), staticFences.GetBuffer(), waitForAll ? VK_TRUE : VK_FALSE, timeoutMSec));
			}
		}
		else
		{
			Vector<VkFence> dynFences;
			RKIT_CHECK(dynFences.Resize(numFences));
			for (size_t i = 0; i < numFences; i++)
				dynFences[i] = static_cast<VulkanBinaryCPUWaitableFence *>(fences[i])->GetFence();

			RKIT_VK_CHECK(m_vkd.vkWaitForFences(m_device, static_cast<uint32_t>(numFences), dynFences.GetBuffer(), waitForAll ? VK_TRUE : VK_FALSE, timeoutMSec));
		}

		return ResultCode::kOK;
	}

	Result VulkanDevice::WaitForDeviceIdle()
	{
		RKIT_VK_CHECK(m_vkd.vkDeviceWaitIdle(m_device));

		// FIXME: Wait for swapchains too
		return ResultCode::kOK;
	}

	VkInstance VulkanDevice::GetInstance() const
	{
		return m_inst;
	}

	const VkAllocationCallbacks *VulkanDevice::GetAllocCallbacks() const
	{
		return m_allocCallbacks;
	}

	const IRenderDeviceCaps &VulkanDevice::GetCaps() const
	{
		return m_caps;
	}

	Result VulkanDevice::CreatePipelineLibraryLoader(UniquePtr<IPipelineLibraryLoader> &outLoader,
		UniquePtr<IPipelineLibraryConfigValidator> &&validator,
		UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart)
	{
		UniquePtr<PipelineLibraryLoaderBase> loader;
		RKIT_CHECK(PipelineLibraryLoaderBase::Create(*this, loader, std::move(validator),
			std::move(package), std::move(packageStream), packageBinaryContentStart));

		outLoader = std::move(loader);

		return ResultCode::kOK;
	}


	const VulkanGlobalAPI &VulkanDevice::GetGlobalAPI() const
	{
		return m_vkg;
	}

	const VulkanInstanceAPI &VulkanDevice::GetInstanceAPI() const
	{
		return m_vki;
	}

	const VulkanDeviceAPI &VulkanDevice::GetDeviceAPI() const
	{
		return m_vkd;
	}

	const VulkanGlobalPlatformAPI &VulkanDevice::GetGlobalPlatformAPI() const
	{
		return m_vkg_p;
	}

	const VulkanInstancePlatformAPI &VulkanDevice::GetInstancePlatformAPI() const
	{
		return m_vki_p;
	}

	const VulkanDevicePlatformAPI &VulkanDevice::GetDevicePlatformAPI() const
	{
		return m_vkd_p;
	}

	const RenderVulkanPhysicalDevice &VulkanDevice::GetPhysDevice() const
	{
		return *m_physDevice;
	}

	IResourcePool<VkSemaphore> &VulkanDevice::GetBinarySemaPool() const
	{
		return *m_semaPool;
	}

	IResourcePool<VkFence> &VulkanDevice::GetFencePool() const
	{
		return *m_fencePool;
	}

	VulkanQueueProxyBase &VulkanDevice::GetQueueByID(size_t queueID)
	{
		return *m_allQueues[queueID];
	}

	size_t VulkanDevice::GetQueueCount() const
	{
		return m_allQueues.Count();
	}

	Result VulkanDevice::CreateSwapChainPrototype(UniquePtr<ISwapChainPrototype> &outSwapChainPrototype, IDisplay &display)
	{
		UniquePtr<VulkanSwapChainPrototypeBase> vkSwapChainPrototype;
		RKIT_CHECK(VulkanSwapChainPrototypeBase::Create(vkSwapChainPrototype, *this, display));

		outSwapChainPrototype = std::move(vkSwapChainPrototype);

		return ResultCode::kOK;
	}

	Result VulkanDevice::CreateSwapChain(UniquePtr<ISwapChain> &outSwapChain, UniquePtr<ISwapChainPrototype> &&prototypeRef, uint8_t numImages, render::RenderTargetFormat fmt, SwapChainWriteBehavior writeBehavior, IBaseCommandQueue &commandQueue)
	{
		if (!prototypeRef.IsValid())
			return ResultCode::kInternalError;

		UniquePtr<ISwapChainPrototype> prototype = std::move(prototypeRef);

		VulkanQueueProxyBase *queue = static_cast<VulkanQueueProxyBase *>(commandQueue.ToInternalCommandQueue());

		uint32_t queueFamily = queue->GetQueueFamily();

		UniquePtr<VulkanSwapChainBase> vkSwapChain;
		RKIT_CHECK(VulkanSwapChainBase::Create(vkSwapChain, *this, *static_cast<VulkanSwapChainPrototypeBase *>(prototype.Get()), numImages, fmt, writeBehavior, *queue));


		outSwapChain = std::move(vkSwapChain);

		return ResultCode::kOK;
	}

	Result VulkanDevice::LoadDeviceAPI()
	{
		RKIT_CHECK(LoadVulkanAPI(m_vkd, FunctionResolver(m_vki, *this)));
		RKIT_CHECK(LoadVulkanAPI(m_vkd_p, FunctionResolver(m_vki, *this)));

		return ResultCode::kOK;
	}

	Result VulkanDevice::CreatePools()
	{
		RKIT_CHECK(CreateResourcePool<VkSemaphore>(m_semaPool, m_semaFactory, true));
		RKIT_CHECK(CreateResourcePool<VkFence>(m_fencePool, m_fenceFactory, true));

		return ResultCode::kOK;
	}

	Result VulkanDeviceBase::CreateDevice(UniquePtr<IRenderDevice> &outDevice, const VulkanGlobalAPI &vkg, const VulkanInstanceAPI &vki, const VulkanGlobalPlatformAPI &vkg_p, const VulkanInstancePlatformAPI &vki_p, VkInstance inst, VkDevice device, const QueueFamilySpec(&queues)[static_cast<size_t>(CommandQueueType::kCount)], const VkAllocationCallbacks *allocCallbacks, const RenderDeviceCaps &caps, const RCPtr<RenderVulkanPhysicalDevice> &physDevice, Vector<StringView> &&enabledExts)
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;

		UniquePtr<IMutex> queueMutex;
		RKIT_CHECK(sysDriver->CreateMutex(queueMutex));

		UniquePtr<VulkanDevice> vkDevice;
		RKIT_CHECK(New<VulkanDevice>(vkDevice, vkg, vki, vkg_p, vki_p, inst, device, allocCallbacks, caps, physDevice, std::move(enabledExts), std::move(queueMutex)));
		
		RKIT_CHECK(vkDevice->LoadDeviceAPI());
		RKIT_CHECK(vkDevice->CreatePools());

		size_t numQueues = 0;
		for (size_t i = 0; i < static_cast<size_t>(CommandQueueType::kCount); i++)
		{
			CommandQueueType queueType = static_cast<CommandQueueType>(i);
			const QueueFamilySpec &spec = queues[i];

			if (spec.m_numQueues > 0)
			{
				RKIT_CHECK(vkDevice->ResolveQueues(queueType, numQueues, spec.m_queueFamily, spec.m_numQueues));

				numQueues += spec.m_numQueues;
			}
		}

		RKIT_CHECK(vkDevice->FinalizeQueueList());

		outDevice = std::move(vkDevice);

		return ResultCode::kOK;
	}
}
