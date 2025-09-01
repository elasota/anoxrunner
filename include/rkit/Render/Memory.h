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

	class MemoryPosition final
	{
	public:
		MemoryPosition();
		MemoryPosition(IMemoryHeap *memHeap, GPUMemoryOffset_t offset);
		MemoryPosition(const MemoryPosition &other) = default;

		IMemoryHeap *GetHeap() const;
		GPUMemoryOffset_t GetOffset() const;

	private:
		IMemoryHeap *m_memHeap = nullptr;
		GPUMemoryOffset_t m_offset = 0;
	};

	class MemoryRegion final
	{
	public:
		MemoryRegion();
		MemoryRegion(const MemoryPosition &position, GPUMemorySize_t size);
		MemoryRegion(const MemoryRegion &other) = default;

		MemoryRegion Subrange(GPUMemoryOffset_t offset, GPUMemorySize_t size) const;

		const MemoryPosition &GetPosition() const;
		IMemoryHeap *GetHeap() const;
		GPUMemoryOffset_t GetOffset() const;
		GPUMemorySize_t GetSize() const;

	private:
		MemoryPosition m_position;
		GPUMemorySize_t m_size = 0;
	};

	class MemoryRequirementsView final
	{
	public:
		typedef rkit::Optional<HeapKey> (*HeapKeyFilterCallback_t)(const void *userdata, const HeapSpec &heapSpec);

		MemoryRequirementsView();
		MemoryRequirementsView(GPUMemorySize_t size, GPUMemoryAlignment_t alignment, const void *userdata, HeapKeyFilterCallback_t heapKeyFilter);
		MemoryRequirementsView(const MemoryRequirementsView &other) = default;

		GPUMemorySize_t Size() const;
		GPUMemoryAlignment_t Alignment() const;
		rkit::Optional<HeapKey> FindSuitableHeap(const HeapSpec &heapSpec) const;

	private:
		GPUMemorySize_t m_size;
		GPUMemoryAlignment_t m_alignment;
		const void *m_userdata;
		HeapKeyFilterCallback_t m_heapKeyFilter;
	};

	struct IMemoryHeap
	{
		virtual ~IMemoryHeap() {}

		virtual MemoryPosition GetStartPosition() const = 0;
		virtual GPUMemorySize_t GetSize() const = 0;
		virtual void *GetCPUPtr() const = 0;

		MemoryRegion GetRegion();
	};
} }

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/Optional.h"

#include "rkit/Render/HeapKey.h"

namespace rkit { namespace render {
	inline MemoryPosition::MemoryPosition()
		: m_memHeap(nullptr)
		, m_offset(0)
	{
	}

	inline MemoryPosition::MemoryPosition(IMemoryHeap *memHeap, GPUMemoryOffset_t offset)
		: m_memHeap(memHeap)
		, m_offset(offset)
	{
	}

	inline IMemoryHeap *MemoryPosition::GetHeap() const
	{
		return m_memHeap;
	}

	inline GPUMemoryOffset_t MemoryPosition::GetOffset() const
	{
		return m_offset;
	}

	inline MemoryRegion::MemoryRegion()
		: m_size(0)
	{
	}

	inline MemoryRegion::MemoryRegion(const MemoryPosition &position, GPUMemorySize_t size)
		: m_position(position)
		, m_size(size)
	{
	}

	inline MemoryRegion MemoryRegion::Subrange(GPUMemoryOffset_t offset, GPUMemorySize_t size) const
	{
		RKIT_ASSERT(offset <= m_size);
		RKIT_ASSERT(size <= (m_size - offset));

		return MemoryRegion(MemoryPosition(m_position.GetHeap(), m_position.GetOffset() + offset), size);
	}

	inline IMemoryHeap *MemoryRegion::GetHeap() const
	{
		return m_position.GetHeap();
	}

	inline GPUMemoryOffset_t MemoryRegion::GetOffset() const
	{
		return m_position.GetOffset();
	}

	inline GPUMemorySize_t MemoryRegion::GetSize() const
	{
		return m_size;
	}

	inline MemoryRequirementsView::MemoryRequirementsView(GPUMemorySize_t size, GPUMemoryAlignment_t alignment, const void *userdata, HeapKeyFilterCallback_t heapKeyFilter)
		: m_size(size)
		, m_alignment(alignment)
		, m_userdata(userdata)
		, m_heapKeyFilter(heapKeyFilter)
	{
	}

	inline GPUMemorySize_t MemoryRequirementsView::Size() const
	{
		return m_size;
	}

	inline GPUMemoryAlignment_t MemoryRequirementsView::Alignment() const
	{
		return m_alignment;
	}

	inline rkit::Optional<HeapKey> MemoryRequirementsView::FindSuitableHeap(const HeapSpec &heapSpec) const
	{
		return m_heapKeyFilter(m_userdata, heapSpec);
	}

	inline MemoryRegion IMemoryHeap::GetRegion()
	{
		return MemoryRegion(GetStartPosition(), GetSize());
	}
} }
