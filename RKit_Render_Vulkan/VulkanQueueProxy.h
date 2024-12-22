#pragma once

#include "rkit/Render/CommandQueue.h"

#include "rkit/Core/ResizableRingBuffer.h"
#include "rkit/Core/Optional.h"

#include "IncludeVulkan.h"
#include "VulkanSync.h"

namespace rkit::render
{
	struct IBaseCommandList;
}

namespace rkit::render::vulkan
{
	struct VulkanDeviceAPI;

	class QueueProxy final : public IGraphicsComputeCommandQueue
	{
	public:
		QueueProxy(IMallocDriver *alloc, VulkanDeviceBase &device, VkQueue queue, const VulkanDeviceAPI &deviceAPI, size_t queueID);

		Result QueueCopy(const Span<ICopyCommandList *> &cmdLists) override;
		Result QueueCompute(const Span<IComputeCommandList *> &cmdLists) override;
		Result QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists) override;
		Result QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists) override;

		Result QueueWaitForOtherQueue(IBaseCommandQueue &otherQueue) override;
		Result Submit(ICPUWaitableFence *cpuWaitableFence) override;

		size_t GetQueueID() const;

	private:
		Result SubmitLocked(ICPUWaitableFence *cpuWaitableFence);

		struct CrossQueueWait
		{
			Optional<FenceID_t> m_crossQueueWaits;
		};

		struct CrossQueueWaitChunkAllocator
		{
			Result Allocate(CrossQueueWait *&waits, size_t count) const;
			void Deallocate(CrossQueueWait *waits) const;

			IMallocDriver *m_alloc = nullptr;
			size_t m_numQueues = 0;
		};

		struct CrossQueueWaitChunk
		{
			explicit CrossQueueWaitChunk(const CrossQueueWaitChunkAllocator *allocator);
			~CrossQueueWaitChunk();

			CrossQueueWait *GetDataAtPosition(size_t addr);
			Result Initialize(size_t size, size_t alignment);

			CrossQueueWait *m_cqWaits = nullptr;
			const CrossQueueWaitChunkAllocator *m_alloc = nullptr;
		};

		struct CrossQueueWaitTraits
		{
			typedef CrossQueueWait *Addr_t;
			typedef size_t AddrOffset_t;
			typedef CrossQueueWaitChunk MemChunk_t;
			typedef CrossQueueWaitChunkAllocator *ChunkAllocator_t;

			static const size_t kMaxAlignment = 1;
			static const size_t kMinimumSize = 1;
			static const size_t kMinimumInfoBlocks = 8;

			static void ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
			static void DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
		};

		struct FenceSync
		{
			Optional<FenceID_t> m_cpuWaitableFence;
			Optional<FenceID_t> *m_crossQueueSemaWaits = nullptr;
			FenceSync *m_nextSync = nullptr;
		};

		template<class T>
		struct SimpleObjAllocator
		{
			explicit SimpleObjAllocator(IMallocDriver *alloc);

			Result Allocate(T *&objs, size_t count) const;
			void Deallocate(T *objs) const;

			IMallocDriver *m_alloc;
		};

		template<class T>
		struct SimpleObjChunk
		{
			explicit SimpleObjChunk(const SimpleObjAllocator<T> *allocator);
			~SimpleObjChunk();

			T *GetDataAtPosition(size_t addr);
			Result Initialize(size_t size, size_t alignment);

			T *m_objs = nullptr;
			const SimpleObjAllocator<T> *m_alloc = nullptr;
		};

		template<class T>
		struct SimpleObjTraits
		{
			typedef T *Addr_t;
			typedef size_t AddrOffset_t;
			typedef SimpleObjChunk<T> MemChunk_t;
			typedef SimpleObjAllocator<T> *ChunkAllocator_t;

			static const size_t kMaxAlignment = 1;
			static const size_t kMinimumSize = 8;
			static const size_t kMinimumInfoBlocks = 8;

			static void ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
			static void DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
		};

		struct SemaChunkAllocator
		{
			explicit SemaChunkAllocator(IMallocDriver *alloc, VulkanDeviceBase &device, const VulkanDeviceAPI &vkd);

			Result Allocate(VkSemaphore *&objs, size_t count) const;
			void Deallocate(VkSemaphore *objs, size_t count) const;

			IMallocDriver *m_alloc;
			VulkanDeviceBase &m_device;
			const VulkanDeviceAPI &m_vkd;
		};

		struct SemaChunk
		{
			explicit SemaChunk(const SemaChunkAllocator *allocator);
			~SemaChunk();

			VkSemaphore *GetDataAtPosition(size_t addr);
			Result Initialize(size_t size, size_t alignment);

			VkSemaphore *m_semas = nullptr;
			size_t m_count = 0;
			const SemaChunkAllocator *m_alloc;
		};

		struct SemaRingTraits
		{
			typedef VkSemaphore *Addr_t;
			typedef size_t AddrOffset_t;
			typedef SemaChunk MemChunk_t;
			typedef SemaChunkAllocator *ChunkAllocator_t;

			static const size_t kMaxAlignment = 1;
			static const size_t kMinimumSize = 1;
			static const size_t kMinimumInfoBlocks = 8;

			static void ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
			static void DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
		};

		struct FenceChunkAllocator
		{
			explicit FenceChunkAllocator(IMallocDriver *alloc, VulkanDeviceBase &device, const VulkanDeviceAPI &vkd);

			Result Allocate(VkFence *&objs, size_t count) const;
			void Deallocate(VkFence *objs, size_t count) const;

			IMallocDriver *m_alloc;
			VulkanDeviceBase &m_device;
			const VulkanDeviceAPI &m_vkd;
		};

		struct FenceChunk
		{
			explicit FenceChunk(const FenceChunkAllocator *allocator);
			~FenceChunk();

			static void DestructContents(VkFence *semas, size_t size);
			VkFence *GetDataAtPosition(size_t addr);
			Result Initialize(size_t size, size_t alignment);

			VkFence *m_fences = nullptr;
			size_t m_count = 0;
			const FenceChunkAllocator *m_alloc = nullptr;
		};

		struct FenceRingTraits
		{
			typedef VkFence *Addr_t;
			typedef size_t AddrOffset_t;
			typedef FenceChunk MemChunk_t;
			typedef FenceChunkAllocator *ChunkAllocator_t;

			static const size_t kMaxAlignment = 1;
			static const size_t kMinimumSize = 1;
			static const size_t kMinimumInfoBlocks = 8;

			static void ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
			static void DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size);
		};

		static const size_t kMaxQueuedCommandLists = 64;
		static const size_t kMaxQueuedSubmits = 32;

		VkCommandBuffer m_commandLists[kMaxQueuedCommandLists];
		VkSubmitInfo m_submitInfos[kMaxQueuedSubmits];

		Result QueueBase(IBaseCommandList &cmdList);

		VulkanDeviceBase *m_device = nullptr;
		VkQueue m_queue = VK_NULL_HANDLE;
		const VulkanDeviceAPI *m_vkd = nullptr;
		size_t m_queueID = 0;

		FenceID_t m_numAllocatedSemaIDs = 0;
		FenceID_t m_numAllocatedFenceIDs = 0;
		FenceID_t m_firstUnretiredFenceID = 0;
		FenceID_t m_firstUnretiredSemaID = 0;

		size_t m_numQueuedCommandLists = 0;
		size_t m_numQueuedSubmitInfos = 0;

		SemaChunkAllocator m_semaphoreAllocator;
		ResizableRingBuffer<SemaRingTraits> m_semaphores;

		FenceChunkAllocator m_fenceAllocator;
		ResizableRingBuffer<FenceRingTraits> m_fences;

		SimpleObjAllocator<FenceSync> m_fenceSyncAllocator;
		ResizableRingBuffer<SimpleObjTraits<FenceSync>> m_pendingFenceChunks;

		CrossQueueWaitChunkAllocator m_crossQueueWaitAllocator;
		ResizableRingBuffer<CrossQueueWaitTraits> m_crossQueueWaits;
	};
}
