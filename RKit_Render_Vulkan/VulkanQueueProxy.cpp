#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

#include "VulkanAPI.h"
#include "VulkanDevice.h"
#include "VulkanCommandList.h"
#include "VulkanQueueProxy.h"

#include <cstring>

namespace rkit::render::vulkan
{
	QueueProxy::QueueProxy(IMallocDriver *alloc, VulkanDeviceBase &device, VkQueue queue, const VulkanDeviceAPI &deviceAPI, size_t queueID)
		: m_semaphoreAllocator(alloc, device, deviceAPI)
		, m_semaphores(alloc, &m_semaphoreAllocator)
		, m_fenceAllocator(alloc, device, deviceAPI)
		, m_fences(alloc, &m_fenceAllocator)
		, m_fenceSyncAllocator(alloc)
		, m_pendingFenceChunks(alloc, &m_fenceSyncAllocator)
		, m_crossQueueWaitAllocator()
		, m_crossQueueWaits(alloc, &m_crossQueueWaitAllocator)
	{
		m_device = &device;
		m_vkd = &deviceAPI;
		m_queue = queue;
		m_queueID = queueID;
	}

	Result QueueProxy::QueueCopy(const Span<ICopyCommandList *> &cmdLists)
	{
		for (ICopyCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueCompute(const Span<IComputeCommandList *> &cmdLists)
	{
		for (IComputeCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueGraphics(const Span<IGraphicsCommandList *> &cmdLists)
	{
		for (IGraphicsCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists)
	{
		for (IGraphicsComputeCommandList *cmdList : cmdLists)
		{
			RKIT_CHECK(QueueBase(*cmdList));
		}

		return ResultCode::kOK;
	}

	Result QueueProxy::QueueWaitForOtherQueue(IBaseCommandQueue &otherQueue)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result QueueProxy::Submit(ICPUWaitableFence *cpuWaitableFence)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result QueueProxy::SubmitLocked(ICPUWaitableFence *cpuWaitableFence)
	{
		return ResultCode::kNotYetImplemented;
	}


	Result QueueProxy::CrossQueueWaitChunkAllocator::Allocate(CrossQueueWait *&waits, size_t count) const
	{
		size_t realCount = 0;
		size_t memSize = 0;
		RKIT_CHECK(SafeMul(realCount, count, m_numQueues));
		RKIT_CHECK(SafeMul(memSize, realCount, sizeof(CrossQueueWait)));

		waits = static_cast<CrossQueueWait *>(m_alloc->Alloc(memSize));
		if (!waits)
			return ResultCode::kOutOfMemory;

		return ResultCode::kOK;
	}

	void QueueProxy::CrossQueueWaitChunkAllocator::Deallocate(CrossQueueWait *waits) const
	{
		m_alloc->Free(waits);
	}

	QueueProxy::CrossQueueWaitChunk::CrossQueueWaitChunk(const CrossQueueWaitChunkAllocator *allocator)
		: m_alloc(allocator)
	{
	}

	QueueProxy::CrossQueueWaitChunk::~CrossQueueWaitChunk()
	{
		m_alloc->Deallocate(m_cqWaits);
	}

	QueueProxy::CrossQueueWait *QueueProxy::CrossQueueWaitChunk::GetDataAtPosition(size_t addr)
	{
		return m_cqWaits + addr * m_alloc->m_numQueues;
	}

	Result QueueProxy::CrossQueueWaitChunk::Initialize(size_t size, size_t alignment)
	{
		return m_alloc->Allocate(m_cqWaits, size);
	}

	void QueueProxy::CrossQueueWaitTraits::ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
		size *= alloc->m_numQueues;
		for (size_t i = 0; i < size; i++)
			new (addr + i) CrossQueueWait();
	}

	void QueueProxy::CrossQueueWaitTraits::DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
		size *= alloc->m_numQueues;
		for (size_t i = 0; i < size; i++)
			addr[i].~CrossQueueWait();
	}

	size_t QueueProxy::GetQueueID() const
	{
		return m_queueID;
	}



	template<class T>
	QueueProxy::SimpleObjAllocator<T>::SimpleObjAllocator(IMallocDriver *alloc)
		: m_alloc(alloc)
	{
	}

	template<class T>
	Result QueueProxy::SimpleObjAllocator<T>::Allocate(T *&objs, size_t count) const
	{
		size_t memSize = 0;
		RKIT_CHECK(SafeMul(memSize, count, sizeof(T)));

		T *allocatedObjs = static_cast<T *>(m_alloc->Alloc(memSize));
		if (!allocatedObjs)
			return ResultCode::kOutOfMemory;

		objs = allocatedObjs;
		return ResultCode::kOK;
	}

	template<class T>
	void QueueProxy::SimpleObjAllocator<T>::Deallocate(T *objs) const
	{
		m_alloc->Free(objs);
	}

	template<class T>
	QueueProxy::SimpleObjChunk<T>::SimpleObjChunk(const SimpleObjAllocator<T> *allocator)
		: m_alloc(allocator)
		, m_objs(nullptr)
	{
	}

	template<class T>
	QueueProxy::SimpleObjChunk<T>::~SimpleObjChunk()
	{
		if (m_objs)
			m_alloc->Deallocate(m_objs);
	}

	template<class T>
	T *QueueProxy::SimpleObjChunk<T>::GetDataAtPosition(size_t addr)
	{
		return m_objs + addr;
	}

	template<class T>
	Result QueueProxy::SimpleObjChunk<T>::Initialize(size_t size, size_t alignment)
	{
		return m_alloc->Allocate(m_objs, size);
	}

	template<class T>
	void QueueProxy::SimpleObjTraits<T>::ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
		for (size_t i = 0; i < size; i++)
			new (addr + i) T();
	}

	template<class T>
	void QueueProxy::SimpleObjTraits<T>::DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
		for (size_t i = 0; i < size; i++)
			addr[i].~T();
	}

	QueueProxy::SemaChunkAllocator::SemaChunkAllocator(IMallocDriver *alloc, VulkanDeviceBase &device, const VulkanDeviceAPI &vkd)
		: m_alloc(alloc)
		, m_device(device)
		, m_vkd(vkd)
	{
	}

	Result QueueProxy::SemaChunkAllocator::Allocate(VkSemaphore *&objs, size_t count) const
	{
		size_t memSize = 0;
		RKIT_CHECK(SafeMul(memSize, count, sizeof(VkSemaphore)));

		VkSemaphore *allocatedObjs = static_cast<VkSemaphore *>(m_alloc->Alloc(memSize));
		for (size_t i = 0; i < count; i++)
			new (allocatedObjs + i) VkSemaphore(VK_NULL_HANDLE);

		objs = allocatedObjs;

		return ResultCode::kOK;
	}

	void QueueProxy::SemaChunkAllocator::Deallocate(VkSemaphore *objs, size_t count) const
	{
		for (size_t i = 0; i < count; i++)
		{
			if (objs[i] != VK_NULL_HANDLE)
				m_vkd.vkDestroySemaphore(m_device.GetDevice(), objs[i], m_device.GetAllocCallbacks());
		}

		m_alloc->Free(objs);
	}

	QueueProxy::SemaChunk::SemaChunk(const SemaChunkAllocator *allocator)
		: m_count(0)
		, m_semas(nullptr)
		, m_alloc(allocator)
	{
	}

	QueueProxy::SemaChunk::~SemaChunk()
	{
		if (m_count > 0)
			m_alloc->Deallocate(m_semas, m_count);
	}

	VkSemaphore *QueueProxy::SemaChunk::GetDataAtPosition(size_t addr)
	{
		return m_semas + addr;
	}

	Result QueueProxy::SemaChunk::Initialize(size_t size, size_t alignment)
	{
		RKIT_CHECK(m_alloc->Allocate(m_semas, size));
		m_count = size;

		return ResultCode::kOK;
	}

	void QueueProxy::SemaRingTraits::ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
	}

	void QueueProxy::SemaRingTraits::DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
	}

	QueueProxy::FenceChunkAllocator::FenceChunkAllocator(IMallocDriver *alloc, VulkanDeviceBase &device, const VulkanDeviceAPI &vkd)
		: m_alloc(alloc)
		, m_device(device)
		, m_vkd(vkd)
	{
	}

	Result QueueProxy::FenceChunkAllocator::Allocate(VkFence *&objs, size_t count) const
	{
		size_t memSize = 0;
		RKIT_CHECK(SafeMul(memSize, count, sizeof(VkFence)));

		VkFence *allocatedObjs = static_cast<VkFence *>(m_alloc->Alloc(memSize));
		for (size_t i = 0; i < count; i++)
			new (allocatedObjs + i) VkFence(VK_NULL_HANDLE);

		objs = allocatedObjs;

		return ResultCode::kOK;
	}

	void QueueProxy::FenceChunkAllocator::Deallocate(VkFence *objs, size_t count) const
	{
		for (size_t i = 0; i < count; i++)
		{
			if (objs[i] != VK_NULL_HANDLE)
				m_vkd.vkDestroyFence(m_device.GetDevice(), objs[i], m_device.GetAllocCallbacks());
		}

		m_alloc->Free(objs);
	}


	QueueProxy::FenceChunk::FenceChunk(const FenceChunkAllocator *allocator)
		: m_fences(nullptr)
		, m_count(0)
		, m_alloc(allocator)
	{
	}

	QueueProxy::FenceChunk::~FenceChunk()
	{
		if (m_count > 0)
			m_alloc->Deallocate(m_fences, m_count);
	}

	VkFence *QueueProxy::FenceChunk::GetDataAtPosition(size_t addr)
	{
		return m_fences + addr;
	}

	Result QueueProxy::FenceChunk::Initialize(size_t size, size_t alignment)
	{
		RKIT_CHECK(m_alloc->Allocate(m_fences, size));
		m_count = size;

		return ResultCode::kOK;
	}

	void QueueProxy::FenceRingTraits::ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
	}

	void QueueProxy::FenceRingTraits::DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t size)
	{
	}

	Result QueueProxy::QueueBase(IBaseCommandList &cmdList)
	{
		// FIXME: Lock
		bool needNewSubmit = false;

		if (m_numQueuedSubmitInfos == 0)
			needNewSubmit = true;
		else
		{
			const VkSubmitInfo &lastSubmitInfo = m_submitInfos[m_numQueuedSubmitInfos - 1];
			if (lastSubmitInfo.signalSemaphoreCount > 0 || lastSubmitInfo.waitSemaphoreCount > 0)
				needNewSubmit = true;
		}

		if (m_numQueuedCommandLists == kMaxQueuedCommandLists || (needNewSubmit && m_numQueuedSubmitInfos == kMaxQueuedSubmits))
		{
			RKIT_CHECK(SubmitLocked(nullptr));
			needNewSubmit = true;

			m_numQueuedCommandLists = 0;	// Quiet analyzer
		}

		VkSubmitInfo *submitInfo = nullptr;
		if (!needNewSubmit)
			submitInfo = &m_submitInfos[m_numQueuedSubmitInfos - 1];
		else
		{
			submitInfo = &m_submitInfos[m_numQueuedSubmitInfos++];
			memset(submitInfo, 0, sizeof(VkSubmitInfo));

			submitInfo->sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo->pCommandBuffers = m_commandLists + m_numQueuedCommandLists;
		}

		submitInfo->commandBufferCount++;

		m_commandLists[m_numQueuedCommandLists++] = static_cast<CommandListProxy *>(cmdList.ToGraphicsComputeCommandList())->GetCommandBuffer();

		return ResultCode::kOK;
	}
}
