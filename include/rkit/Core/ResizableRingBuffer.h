#pragma once

#include "NoCopy.h"

#include <cstddef>

namespace rkit
{
	struct IMallocDriver;

	template<class TTraits>
	class ResizableRingBufferHandle;

	class ResizableRingBufferCPUMemChunk
	{
	public:
		explicit ResizableRingBufferCPUMemChunk(IMallocDriver *alloc);
		~ResizableRingBufferCPUMemChunk();

		Result Initialize(size_t size, size_t alignment);
		void *GetDataAtPosition(size_t offset) const;

	private:
		void *m_alignedMemory;
		void *m_baseAddr;
		IMallocDriver *m_alloc;
	};

	struct DefaultResizableRingBufferTraits
	{
		static const bool kShrinkableInfoTable = false;
		static const bool kShrinkable = false;

		typedef size_t AddrOffset_t;
		typedef void *Addr_t;

		typedef IMallocDriver *ChunkAllocator_t;
		typedef ResizableRingBufferCPUMemChunk MemChunk_t;

		static const size_t kMinimumSize = 8;
		static const size_t kMinimumInfoBlocks = 8;
		static const AddrOffset_t kMaxAlignment = 16;

		static void ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t memorySize);
		static void DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t memorySize);
	};

	template<class TTraits>
	struct ResizableRingBufferInfoBlock
	{
		typedef typename TTraits::AddrOffset_t AddrOffset_t;
		typedef typename TTraits::MemChunk_t MemChunk_t;

		ResizableRingBufferInfoBlock();

		MemChunk_t *m_memChunk;
		AddrOffset_t m_offset;
		AddrOffset_t m_size;
	};

	template<class TTraits>
	struct IResizableRingBuffer
	{
		typedef typename TTraits::AddrOffset_t AddrOffset_t;
		typedef typename TTraits::MemChunk_t MemChunk_t;

		virtual ~IResizableRingBuffer() {}

		struct InfoBlockChunk
		{
		};

		virtual Result Allocate(AddrOffset_t size, AddrOffset_t alignment, ResizableRingBufferHandle<TTraits> &outHandle, const MemChunk_t *&outMemChunk, AddrOffset_t &outAddrOffset) = 0;
		virtual void Dispose(const ResizableRingBufferHandle<TTraits> &handle) = 0;
	};

	template<class TTraits>
	class ResizableRingBuffer final : public IResizableRingBuffer<TTraits>, public NoCopy
	{
	public:
		typedef typename TTraits::AddrOffset_t AddrOffset_t;
		typedef typename TTraits::MemChunk_t MemChunk_t;
		typedef typename TTraits::ChunkAllocator_t ChunkAllocator_t;
		typedef ResizableRingBufferInfoBlock<TTraits> InfoBlock_t;

		explicit ResizableRingBuffer(IMallocDriver *alloc, const ChunkAllocator_t &allocator);
		~ResizableRingBuffer();

		Result Allocate(AddrOffset_t size, AddrOffset_t alignment, ResizableRingBufferHandle<TTraits> &outHandle, const MemChunk_t *&outMemChunk, AddrOffset_t &outAddrOffset) override;
		void Dispose(const ResizableRingBufferHandle<TTraits> &handle) override;

	private:
		struct LinkedMemChunk final : private MemChunk_t
		{
			explicit LinkedMemChunk(ResizableRingBuffer<TTraits> *owner, const ChunkAllocator_t &chunkAllocator, AddrOffset_t capacity);

			MemChunk_t *GetMemChunk();
			const MemChunk_t *GetMemChunk() const;

			static const LinkedMemChunk *FromMemChunk(const MemChunk_t *memChunk);
			static LinkedMemChunk *FromMemChunk(MemChunk_t *memChunk);

			size_t m_numActiveAllocations;

			AddrOffset_t m_capacity;
			AddrOffset_t m_firstUsedAddress;
			AddrOffset_t m_usedAddressSize;

			ResizableRingBuffer<TTraits> *m_owner;
			LinkedMemChunk *m_next;
		};

		struct FlaggedInfoBlock : public ResizableRingBufferInfoBlock<TTraits>
		{
			FlaggedInfoBlock();

			bool m_isAllocated;
		};

		struct LinkedInfoBlockChunk : public IResizableRingBuffer<TTraits>::InfoBlockChunk
		{
			explicit LinkedInfoBlockChunk(ResizableRingBuffer<TTraits> *owner, FlaggedInfoBlock *infoBlocks, size_t numInfoBlocks);

			FlaggedInfoBlock *m_infoBlocks;
			size_t m_numInfoBlocks;

			size_t m_firstActiveInfoBlock;
			size_t m_numActiveInfoBlocks;

			ResizableRingBuffer<TTraits> *m_owner;
			LinkedInfoBlockChunk *m_next;
		};

		LinkedMemChunk *m_firstMemChunk;
		LinkedMemChunk *m_lastMemChunk;

		LinkedInfoBlockChunk *m_firstInfoBlockChunk;
		LinkedInfoBlockChunk *m_lastInfoBlockChunk;

		ChunkAllocator_t m_chunkAllocator;
		IMallocDriver *m_alloc;
	};

	template<class TTraits>
	class ResizableRingBufferHandle
	{
	public:
		friend class ResizableRingBuffer<TTraits>;

		ResizableRingBufferHandle();
		ResizableRingBufferHandle(typename IResizableRingBuffer<TTraits>::InfoBlockChunk *infoBlockChunk, size_t infoBlockIndex);

		void Clear();

	private:
		typename IResizableRingBuffer<TTraits>::InfoBlockChunk *m_infoBlockChunk;
		size_t m_infoBlockIndex;
	};
}

#include "Algorithm.h"
#include "NewDelete.h"
#include "Result.h"
#include "RKitAssert.h"

template<class TTraits>
rkit::ResizableRingBuffer<TTraits>::ResizableRingBuffer(IMallocDriver *alloc, const ChunkAllocator_t &chunkAllocator)
	: m_firstMemChunk(nullptr)
	, m_lastMemChunk(nullptr)
	, m_firstInfoBlockChunk(nullptr)
	, m_lastInfoBlockChunk(nullptr)
	, m_chunkAllocator(chunkAllocator)
	, m_alloc(alloc)
{
}

template<class TTraits>
rkit::ResizableRingBuffer<TTraits>::~ResizableRingBuffer()
{
	LinkedInfoBlockChunk *infoBlockChunk = m_firstInfoBlockChunk;

	while (infoBlockChunk != nullptr)
	{
		FlaggedInfoBlock *infoBlocks = infoBlockChunk->m_infoBlocks;
		size_t numActiveInfoBlocks = infoBlockChunk->m_numActiveInfoBlocks;
		size_t numInfoBlocks = infoBlockChunk->m_numInfoBlocks;
		size_t firstInfoBlock = infoBlockChunk->m_firstActiveInfoBlock;

		for (size_t i = 0; i < numActiveInfoBlocks; i++)
		{
			size_t infoBlockIndex = (i + firstInfoBlock) % numInfoBlocks;
			FlaggedInfoBlock &infoBlock = infoBlocks[infoBlockIndex];

			if (infoBlock.m_isAllocated)
				TTraits::DeactivateContents(m_chunkAllocator, infoBlock.m_memChunk->GetDataAtPosition(infoBlock.m_offset), infoBlock.m_size);
		}

		// Info blocks are POD so just delete them
		LinkedInfoBlockChunk *nextChunk = infoBlockChunk->m_next;
		m_alloc->Free(infoBlockChunk);
		infoBlockChunk = nextChunk;
	}

	LinkedMemChunk *memChunk = m_firstMemChunk;
	while (memChunk != nullptr)
	{
		LinkedMemChunk *nextChunk = memChunk->m_next;

		memChunk->~LinkedMemChunk();
		m_alloc->Free(memChunk);

		memChunk = nextChunk;
	}
}

template<class TTraits>
rkit::Result rkit::ResizableRingBuffer<TTraits>::Allocate(AddrOffset_t size, AddrOffset_t alignment, ResizableRingBufferHandle<TTraits> &outHandle, const MemChunk_t *&outMemChunk, AddrOffset_t &outAddrOffset)
{
	RKIT_ASSERT(alignment != 0);
	RKIT_ASSERT(alignment <= TTraits::kMaxAlignment);
	RKIT_ASSERT(size != 0);

	if (size == 0)
		size = 1;

	if (alignment == 0)
		alignment = 1;

	bool memBlockFits = false;
	AddrOffset_t memBlockLoc = 0;

	if (m_lastMemChunk)
	{
		AddrOffset_t firstAddressDistanceFromEnd = m_lastMemChunk->m_capacity - m_lastMemChunk->m_firstUsedAddress;

		if (firstAddressDistanceFromEnd <= m_lastMemChunk->m_usedAddressSize)
		{
			// Block wraps around the end
			AddrOffset_t tailMemAddress = (m_lastMemChunk->m_firstUsedAddress + m_lastMemChunk->m_usedAddressSize) - m_lastMemChunk->m_capacity;
			AddrOffset_t tailSpaceAvailable = m_lastMemChunk->m_firstUsedAddress - tailMemAddress;

			AddrOffset_t memBlockPadding = alignment - (tailMemAddress % alignment);
			if (memBlockPadding == alignment)
				memBlockPadding = 0;

			if (tailSpaceAvailable >= memBlockPadding && (tailSpaceAvailable - memBlockPadding) >= size)
			{
				memBlockFits = true;
				memBlockLoc = tailMemAddress + memBlockPadding;
			}
		}
		else
		{
			// Block doesn't wrap
			AddrOffset_t tailMemAddress = m_lastMemChunk->m_firstUsedAddress + m_lastMemChunk->m_usedAddressSize;
			AddrOffset_t tailSpaceAvailable = m_lastMemChunk->m_capacity - m_lastMemChunk->m_firstUsedAddress - m_lastMemChunk->m_usedAddressSize;

			AddrOffset_t memBlockPadding = alignment - (tailMemAddress % alignment);
			if (memBlockPadding == alignment)
				memBlockPadding = 0;

			if (tailSpaceAvailable >= memBlockPadding && (tailSpaceAvailable - memBlockPadding) >= size)
			{
				// Can fit in tail space
				memBlockFits = true;
				memBlockLoc = tailMemAddress + memBlockPadding;
			}
			else
			{
				// Can't fit in tail space
				if (m_lastMemChunk->m_firstUsedAddress >= size)
				{
					// Can fit in head space
					memBlockFits = true;
					memBlockLoc = 0;
				}
			}
		}
	}

	bool infoBlocksFit = false;
	size_t infoBlockLoc = 0;

	if (m_lastInfoBlockChunk)
	{
		if (m_lastInfoBlockChunk->m_numActiveInfoBlocks < m_lastInfoBlockChunk->m_numInfoBlocks)
		{
			infoBlocksFit = true;
			infoBlockLoc = (m_lastInfoBlockChunk->m_numActiveInfoBlocks + m_lastInfoBlockChunk->m_firstActiveInfoBlock) % m_lastInfoBlockChunk->m_numInfoBlocks;
		}
	}

	if (!memBlockFits)
	{
		AddrOffset_t newSize = TTraits::kMinimumSize;

		if (m_lastMemChunk)
			newSize = m_lastMemChunk->m_capacity;

		while (newSize < size)
		{
			if (std::numeric_limits<AddrOffset_t>::max() / static_cast<AddrOffset_t>(2) < newSize)
			{
				newSize = std::numeric_limits<AddrOffset_t>::max();
				break;
			}

			newSize *= 2;
		}

		UniquePtr<LinkedMemChunk> memChunk;
		RKIT_CHECK(NewWithAlloc<LinkedMemChunk>(memChunk, m_alloc, this, m_chunkAllocator, newSize));

		RKIT_CHECK(memChunk->GetMemChunk()->Initialize(newSize, TTraits::kMaxAlignment));

		LinkedMemChunk *detached = memChunk.Detach().m_obj;
		if (m_lastMemChunk)
			m_lastMemChunk->m_next = detached;

		m_lastMemChunk = detached;

		if (!m_firstMemChunk)
			m_firstMemChunk = detached;

		memBlockLoc = 0;
	}

	if (!infoBlocksFit)
	{
		size_t paddedInfoBlockChunkSize = sizeof(LinkedInfoBlockChunk);
		paddedInfoBlockChunkSize += alignof(FlaggedInfoBlock) - 1;
		paddedInfoBlockChunkSize -= paddedInfoBlockChunkSize % alignof(FlaggedInfoBlock);

		const size_t maxInfoBlocks = (std::numeric_limits<AddrOffset_t>::max() - paddedInfoBlockChunkSize) / sizeof(FlaggedInfoBlock);

		AddrOffset_t newInfoBlockCount = TTraits::kMinimumInfoBlocks;

		if (m_lastInfoBlockChunk)
		{
			newInfoBlockCount = m_lastInfoBlockChunk->m_numInfoBlocks;
			if (maxInfoBlocks / static_cast<AddrOffset_t>(2) < m_lastInfoBlockChunk->m_numInfoBlocks)
				newInfoBlockCount = maxInfoBlocks;
			else
				newInfoBlockCount = m_lastInfoBlockChunk->m_numInfoBlocks * static_cast<AddrOffset_t>(2);
		}

		size_t infoBlockAllocSize = paddedInfoBlockChunkSize + sizeof(FlaggedInfoBlock) * newInfoBlockCount;

		void *mem = m_alloc->Alloc(infoBlockAllocSize);
		if (!mem)
			return ResultCode::kOutOfMemory;

		LinkedInfoBlockChunk *linkedChunk = static_cast<LinkedInfoBlockChunk *>(mem);
		FlaggedInfoBlock *infoBlocks = reinterpret_cast<FlaggedInfoBlock *>(reinterpret_cast<char *>(mem) + paddedInfoBlockChunkSize);

		for (size_t i = 0; i < newInfoBlockCount; i++)
			new (infoBlocks + i) FlaggedInfoBlock();

		new (linkedChunk) LinkedInfoBlockChunk(this, infoBlocks, newInfoBlockCount);

		if (m_lastInfoBlockChunk)
			m_lastInfoBlockChunk->m_next = linkedChunk;

		m_lastInfoBlockChunk = linkedChunk;

		if (!m_firstInfoBlockChunk)
			m_firstInfoBlockChunk = linkedChunk;

		infoBlockLoc = 0;
	}

	m_lastInfoBlockChunk->m_numActiveInfoBlocks++;

	FlaggedInfoBlock &infoBlock = m_lastInfoBlockChunk->m_infoBlocks[infoBlockLoc];

	infoBlock.m_isAllocated = true;
	infoBlock.m_memChunk = m_lastMemChunk->GetMemChunk();
	infoBlock.m_size = size;
	infoBlock.m_offset = memBlockLoc;

	TTraits::ActivateContents(m_chunkAllocator, infoBlock.m_memChunk->GetDataAtPosition(memBlockLoc), size);

	LinkedMemChunk &memChunk = *m_lastMemChunk;

	AddrOffset_t extendedTailAddress = memBlockLoc + size;

	// memBlockLoc == base address when block is empty
	if (memBlockLoc < memChunk.m_firstUsedAddress)
		extendedTailAddress += memChunk.m_capacity;

	memChunk.m_usedAddressSize = extendedTailAddress - memChunk.m_firstUsedAddress;
	m_lastMemChunk->m_numActiveAllocations++;

	outMemChunk = memChunk.GetMemChunk();
	outAddrOffset = memBlockLoc;

	outHandle = ResizableRingBufferHandle<TTraits>(m_lastInfoBlockChunk, infoBlockLoc);

	return ResultCode::kOK;
}

template<class TTraits>
void rkit::ResizableRingBuffer<TTraits>::Dispose(const ResizableRingBufferHandle<TTraits> &handle)
{
	LinkedInfoBlockChunk *infoBlockChunk = static_cast<LinkedInfoBlockChunk *>(handle.m_infoBlockChunk);

	if (!infoBlockChunk)
		return;

	size_t infoBlockIndex = handle.m_infoBlockIndex;

	FlaggedInfoBlock *deallocatedInfoBlock = infoBlockChunk->m_infoBlocks + infoBlockIndex;

	RKIT_ASSERT(LinkedMemChunk::FromMemChunk(deallocatedInfoBlock->m_memChunk)->m_owner == this);
	RKIT_ASSERT(deallocatedInfoBlock->m_isAllocated);

	deallocatedInfoBlock->m_isAllocated = false;

	TTraits::DeactivateContents(m_chunkAllocator, deallocatedInfoBlock->m_memChunk->GetDataAtPosition(deallocatedInfoBlock->m_offset), deallocatedInfoBlock->m_size);

	// Remove from info block chunk
	for (;;)
	{
		LinkedInfoBlockChunk *headInfoBlockChunk = m_firstInfoBlockChunk;
		if (headInfoBlockChunk->m_numActiveInfoBlocks == 0)
			break;

		FlaggedInfoBlock &headInfoBlock = headInfoBlockChunk->m_infoBlocks[headInfoBlockChunk->m_firstActiveInfoBlock];

		if (headInfoBlock.m_isAllocated)
		{
			// This is the new head allocation in the memory block that it belongs to
			LinkedMemChunk *headMemChunk = this->m_firstMemChunk;

			RKIT_ASSERT(headInfoBlock.m_memChunk == headMemChunk->GetMemChunk());

			AddrOffset_t endAddressExtended = (headMemChunk->m_firstUsedAddress + headMemChunk->m_usedAddressSize);
			AddrOffset_t startAddressExtended = headInfoBlock.m_offset;
			if (startAddressExtended < headMemChunk->m_firstUsedAddress)
				startAddressExtended += headMemChunk->m_capacity;

			headMemChunk->m_usedAddressSize = endAddressExtended - startAddressExtended;
			headMemChunk->m_firstUsedAddress = headInfoBlock.m_offset;
			break;
		}

		RKIT_ASSERT(headInfoBlock.m_memChunk == m_firstMemChunk->GetMemChunk());

		LinkedMemChunk *headMemChunk = LinkedMemChunk::FromMemChunk(headInfoBlock.m_memChunk);

		headMemChunk->m_numActiveAllocations--;

		while (m_firstMemChunk->m_numActiveAllocations == 0)
		{
			LinkedMemChunk *nextChunk = m_firstMemChunk->m_next;

			if (m_firstMemChunk == m_lastMemChunk)
			{
				m_firstMemChunk->m_firstUsedAddress = 0;
				m_firstMemChunk->m_usedAddressSize = 0;
				break;
			}
			else
			{
				m_firstMemChunk->~LinkedMemChunk();
				m_alloc->Free(headMemChunk);

				m_firstMemChunk = nextChunk;
			}
		}

		headInfoBlockChunk->m_firstActiveInfoBlock++;
		if (headInfoBlockChunk->m_firstActiveInfoBlock == headInfoBlockChunk->m_numInfoBlocks)
			headInfoBlockChunk->m_firstActiveInfoBlock = 0;

		headInfoBlockChunk->m_numActiveInfoBlocks--;

		if (headInfoBlockChunk->m_numActiveInfoBlocks == 0)
		{
			if (headInfoBlockChunk == m_lastInfoBlockChunk)
				headInfoBlockChunk->m_firstActiveInfoBlock = 0;
			else
			{
				m_firstInfoBlockChunk = headInfoBlockChunk->m_next;
				headInfoBlockChunk->~LinkedInfoBlockChunk();
				m_alloc->Free(headInfoBlockChunk);
			}
		}
	}
}



inline rkit::ResizableRingBufferCPUMemChunk::ResizableRingBufferCPUMemChunk(IMallocDriver *alloc)
	: m_alloc(alloc)
	, m_alignedMemory(nullptr)
	, m_baseAddr(nullptr)
{
}

inline rkit::ResizableRingBufferCPUMemChunk::~ResizableRingBufferCPUMemChunk()
{
	if (m_baseAddr)
		m_alloc->Free(m_baseAddr);
}

inline rkit::Result rkit::ResizableRingBufferCPUMemChunk::Initialize(size_t size, size_t alignment)
{
	m_baseAddr = m_alloc->Alloc(size + alignment - 1);

	if (!m_baseAddr)
		return ResultCode::kOutOfMemory;

	uintptr_t addrPtr = reinterpret_cast<uintptr_t>(m_baseAddr);
	addrPtr += alignment - 1;
	addrPtr -= addrPtr % alignment;

	m_alignedMemory = reinterpret_cast<void *>(addrPtr);

	return ResultCode::kOK;
}

inline void *rkit::ResizableRingBufferCPUMemChunk::GetDataAtPosition(size_t offset) const
{
	return static_cast<char *>(m_alignedMemory) + offset;
}

inline void rkit::DefaultResizableRingBufferTraits::ActivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t memorySize)
{
}

inline void rkit::DefaultResizableRingBufferTraits::DeactivateContents(ChunkAllocator_t alloc, Addr_t addr, AddrOffset_t memorySize)
{
}


template<class TTraits>
inline rkit::ResizableRingBufferHandle<TTraits>::ResizableRingBufferHandle()
	: m_infoBlockChunk(nullptr)
	, m_infoBlockIndex(0)
{
}

template<class TTraits>
inline rkit::ResizableRingBufferHandle<TTraits>::ResizableRingBufferHandle(typename IResizableRingBuffer<TTraits>::InfoBlockChunk *infoBlockChunk, size_t infoBlockIndex)
	: m_infoBlockChunk(infoBlockChunk)
	, m_infoBlockIndex(infoBlockIndex)
{
}

template<class TTraits>
inline rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk::LinkedMemChunk(ResizableRingBuffer<TTraits> *owner, const ChunkAllocator_t &chunkAllocator, AddrOffset_t capacity)
	: MemChunk_t(chunkAllocator)
	, m_numActiveAllocations(0)
	, m_capacity(capacity)
	, m_firstUsedAddress(0)
	, m_usedAddressSize(0)
	, m_owner(owner)
	, m_next(nullptr)
{
}


template<class TTraits>
inline typename rkit::ResizableRingBuffer<TTraits>::MemChunk_t *rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk::GetMemChunk()
{
	return this;
}

template<class TTraits>
inline const typename rkit::ResizableRingBuffer<TTraits>::MemChunk_t *rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk::GetMemChunk() const
{
	return this;
}

template<class TTraits>
inline const typename rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk *rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk::FromMemChunk(const MemChunk_t *memChunk)
{
	return static_cast<const LinkedMemChunk *>(memChunk);
}

template<class TTraits>
inline typename rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk *rkit::ResizableRingBuffer<TTraits>::LinkedMemChunk::FromMemChunk(MemChunk_t *memChunk)
{
	return static_cast<LinkedMemChunk *>(memChunk);
}


template<class TTraits>
rkit::ResizableRingBuffer<TTraits>::LinkedInfoBlockChunk::LinkedInfoBlockChunk(ResizableRingBuffer<TTraits> *owner, FlaggedInfoBlock *infoBlocks, size_t numInfoBlocks)
	: m_infoBlocks(infoBlocks)
	, m_numInfoBlocks(numInfoBlocks)
	, m_firstActiveInfoBlock(0)
	, m_numActiveInfoBlocks(0)
	, m_owner(owner)
	, m_next(nullptr)
{
}


template<class TTraits>
rkit::ResizableRingBuffer<TTraits>::FlaggedInfoBlock::FlaggedInfoBlock()
	: m_isAllocated(false)
{
}

template<class TTraits>
rkit::ResizableRingBufferInfoBlock<TTraits>::ResizableRingBufferInfoBlock()
	: m_memChunk(nullptr)
	, m_offset(0)
	, m_size(0)
{
}
