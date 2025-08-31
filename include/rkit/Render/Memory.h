#pragma once

#include "MemoryProtos.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace rkit { namespace render {
	struct IMemoryHeap;
	class HeapKey;
	struct HeapSpec;

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

	class MemoryRequirementsView
	{
	public:
		typedef rkit::Optional<HeapKey> (*HeapKeyFilterCallback_t)(const void *userdata, const HeapSpec &heapSpec);

		MemoryRequirementsView();
		MemoryRequirementsView(rkit::render::GPUMemorySize_t size, rkit::render::GPUMemoryAlignment_t alignment, const void *userdata, HeapKeyFilterCallback_t heapKeyFilter);
		MemoryRequirementsView(const MemoryRequirementsView &other) = default;

		rkit::render::GPUMemorySize_t Size() const;
		rkit::render::GPUMemoryAlignment_t Alignment() const;
		rkit::Optional<HeapKey> FindSuitableHeap(const HeapSpec &heapSpec) const;

	private:
		rkit::render::GPUMemorySize_t m_size;
		rkit::render::GPUMemoryAlignment_t m_alignment;
		const void *m_userdata;
		HeapKeyFilterCallback_t m_heapKeyFilter;
	};
} }

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/Optional.h"

#include "rkit/Render/HeapKey.h"

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

		return MemoryRegion(m_memHeap, m_offset + offset, size);
	}

	inline MemoryRequirementsView::MemoryRequirementsView(rkit::render::GPUMemorySize_t size, rkit::render::GPUMemoryAlignment_t alignment, const void *userdata, HeapKeyFilterCallback_t heapKeyFilter)
		: m_size(size)
		, m_alignment(alignment)
		, m_userdata(userdata)
		, m_heapKeyFilter(heapKeyFilter)
	{
	}

	inline rkit::render::GPUMemorySize_t MemoryRequirementsView::Size() const
	{
		return m_size;
	}

	inline rkit::render::GPUMemoryAlignment_t MemoryRequirementsView::Alignment() const
	{
		return m_alignment;
	}

	inline rkit::Optional<HeapKey> MemoryRequirementsView::FindSuitableHeap(const HeapSpec &heapSpec) const
	{
		return m_heapKeyFilter(m_userdata, heapSpec);
	}
} }
