#pragma once

#include "rkit/Core/MallocDriver.h"

namespace rkit
{
	struct IMemMapDriver;
}

namespace rkit { namespace mem {
	// Simple mmap malloc driver for booting the Mem module
	class SimpleMemMapMallocDriver final : public IMallocDriver
	{
	public:
		explicit SimpleMemMapMallocDriver(IMemMapDriver &mmapDriver);

		void *InternalAlloc(size_t size) override;
		void *InternalRealloc(void *ptr, size_t size) override;
		void InternalFree(void *ptr) override;

	private:
		SimpleMemMapMallocDriver() = delete;
		SimpleMemMapMallocDriver(const SimpleMemMapMallocDriver &) = delete;

		size_t GetPaddedMetadataSize() const;

		IMemMapDriver &m_mmapDriver;
		size_t m_pageSize;
	};
} }

#include <limits>

#include "rkit/Mem/MemMapDriver.h"
#include "rkit/Core/Algorithm.h"

namespace rkit { namespace mem {
	inline SimpleMemMapMallocDriver::SimpleMemMapMallocDriver(IMemMapDriver &mmapDriver)
		: m_mmapDriver(mmapDriver)
		, m_pageSize(static_cast<size_t>(1) << mmapDriver.GetPageType(0).m_pageSizePO2)
	{
	}

	inline void *SimpleMemMapMallocDriver::InternalAlloc(size_t size)
	{
		const size_t pageSizeMask = (m_pageSize - 1);
		const size_t metadataSize = GetPaddedMetadataSize();

		size_t remainingSize = std::numeric_limits<size_t>::max();
		remainingSize -= (remainingSize & pageSizeMask);

		size_t initialRemainingSize = remainingSize;

		remainingSize -= metadataSize;

		if (size > remainingSize)
			return nullptr;

		remainingSize -= size;
		remainingSize -= (remainingSize & pageSizeMask);

		const size_t completeSize = initialRemainingSize - remainingSize;

		MemMapPageRange pageRange = {};
		bool succeeded = m_mmapDriver.CreateMapping(pageRange, completeSize, 0, MemMapState::kReadWrite);
		if (!succeeded)
			return nullptr;

		uint8_t *allocatedBytes = static_cast<uint8_t *>(pageRange.m_base);
		MemMapPageRange *metadata = reinterpret_cast<MemMapPageRange *>(allocatedBytes);
		*metadata = pageRange;

		return allocatedBytes + metadataSize;
	}

	inline void *SimpleMemMapMallocDriver::InternalRealloc(void *ptr, size_t size)
	{
		void *newBuffer = Alloc(size);
		if (!newBuffer)
			return nullptr;

		const size_t metadataSize = GetPaddedMetadataSize();

		uint8_t *allocatedBytes = static_cast<uint8_t *>(ptr) - metadataSize;

		const MemMapPageRange *oldMetadata = reinterpret_cast<const MemMapPageRange *>(allocatedBytes);
		const size_t oldSize = oldMetadata->m_size - metadataSize;

		const size_t copySize = Min(oldSize, size);
		CopySpanNonOverlapping(Span<uint8_t>(static_cast<uint8_t *>(newBuffer), copySize), ConstSpan<uint8_t>(static_cast<const uint8_t *>(ptr), copySize));

		Free(ptr);

		return newBuffer;
	}

	inline void SimpleMemMapMallocDriver::InternalFree(void *ptr)
	{
		const size_t metadataSize = GetPaddedMetadataSize();

		uint8_t *allocatedBytes = static_cast<uint8_t *>(ptr) - metadataSize;

		const MemMapPageRange *metadata = reinterpret_cast<const MemMapPageRange *>(allocatedBytes);

		m_mmapDriver.ReleaseMapping(*metadata);
	}


	inline size_t SimpleMemMapMallocDriver::GetPaddedMetadataSize() const
	{
		const size_t baseSize = sizeof(MemMapPageRange);
		const size_t trailingBits = (baseSize & (kDefaultAllocAlignment - 1u));
		const size_t padding = (trailingBits == 0) ? 0 : (kDefaultAllocAlignment - trailingBits);

		return baseSize + padding;
	}
} }
