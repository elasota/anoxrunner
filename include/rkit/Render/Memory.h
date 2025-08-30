#pragma once

#include "MemoryProtos.h"

namespace rkit { namespace render {
	struct IMemoryHeap;

	class MemoryRegion final
	{
	public:
		MemoryRegion();
		MemoryRegion(rkit::render::IMemoryHeap *memHeap, rkit::render::GPUMemoryOffset_t offset, rkit::render::GPUMemorySize_t size);
		MemoryRegion(const MemoryRegion &other) = default;

		MemoryRegion Subrange(rkit::render::GPUMemoryOffset_t offset, rkit::render::GPUMemorySize_t size) const;

	private:
		rkit::render::IMemoryHeap *m_memHeap = nullptr;
		rkit::render::GPUMemoryOffset_t m_offset = 0;
		rkit::render::GPUMemorySize_t m_size = 0;
	};
} }

#include "rkit/Core/RKitAssert.h"

namespace rkit { namespace render {
	inline MemoryRegion::MemoryRegion()
		: m_memHeap(nullptr)
		, m_offset(0)
		, m_size(0)
	{
	}

	inline MemoryRegion::MemoryRegion(rkit::render::IMemoryHeap *memHeap, rkit::render::GPUMemoryOffset_t offset, rkit::render::GPUMemorySize_t size)
		: m_memHeap(memHeap)
		, m_offset(offset)
		, m_size(size)
	{
	}

	inline MemoryRegion MemoryRegion::Subrange(rkit::render::GPUMemoryOffset_t offset, rkit::render::GPUMemorySize_t size) const
	{
		RKIT_ASSERT(offset <= m_size);
		RKIT_ASSERT(size <= (m_size - offset));

		return MemoryRegion(offset, size);
	}
} }
