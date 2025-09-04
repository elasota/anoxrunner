#pragma once

#include "MemoryProtos.h"

namespace rkit
{
	template<class T>
	class Optional;
}

namespace rkit { namespace render {
	struct IMemoryHeap;
	struct IMemoryAllocation;
	class HeapKey;
	struct HeapSpec;

	class MemoryAddress final
	{
	public:
		MemoryAddress();
		MemoryAddress(IMemoryHeap &memHeap, GPUMemoryOffset_t offset);
		MemoryAddress(const MemoryAddress &other) = default;

		IMemoryHeap *GetHeap() const;
		GPUMemoryOffset_t GetOffset() const;

	private:
		IMemoryHeap *m_memHeap = nullptr;
		GPUMemoryOffset_t m_offset = 0;
	};

	class MemoryPosition final
	{
	public:
		MemoryPosition();
		MemoryPosition(IMemoryAllocation &memHeap, GPUMemoryOffset_t offset);
		MemoryPosition(const MemoryPosition &other) = default;

		IMemoryAllocation *GetAllocation() const;
		GPUMemoryOffset_t GetOffset() const;

		MemoryPosition operator+(GPUMemoryOffset_t offset) const;

		operator MemoryAddress() const;

	private:
		IMemoryAllocation *m_memAllocation = nullptr;
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
		IMemoryAllocation *GetAllocation() const;
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

	struct IMemoryAllocation
	{
		virtual ~IMemoryAllocation() {}

		virtual MemoryAddress GetBaseAddress() const = 0;
	
		virtual GPUMemorySize_t GetSize() const = 0;
		virtual void *GetCPUPtr() const = 0;

		MemoryRegion GetRegion() const;
	};

	struct IMemoryHeap : public IMemoryAllocation
	{
		virtual ~IMemoryHeap() {}

		MemoryAddress GetBaseAddress() const override final;
	};
} }

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/Optional.h"

#include "rkit/Render/HeapKey.h"

namespace rkit { namespace render {
	inline MemoryAddress::MemoryAddress()
		: m_memHeap(nullptr)
		, m_offset(0)
	{
	}

	inline MemoryAddress::MemoryAddress(IMemoryHeap &memHeap, GPUMemoryOffset_t offset)
		: m_memHeap(&memHeap)
		, m_offset(offset)
	{
		RKIT_ASSERT(offset <= memHeap.GetSize());
	}

	inline IMemoryHeap *MemoryAddress::GetHeap() const
	{
		return m_memHeap;
	}

	inline GPUMemoryOffset_t MemoryAddress::GetOffset() const
	{
		return m_offset;
	}

	inline MemoryPosition::MemoryPosition()
		: m_memAllocation(nullptr)
		, m_offset(0)
	{
	}

	inline MemoryPosition::MemoryPosition(IMemoryAllocation &memAlloc, GPUMemoryOffset_t offset)
		: m_memAllocation(&memAlloc)
		, m_offset(offset)
	{
	}

	inline IMemoryAllocation *MemoryPosition::GetAllocation() const
	{
		return m_memAllocation;
	}

	inline GPUMemoryOffset_t MemoryPosition::GetOffset() const
	{
		return m_offset;
	}

	inline MemoryPosition::operator MemoryAddress() const
	{
		const MemoryAddress baseAddress = m_memAllocation->GetBaseAddress();
		return MemoryAddress(*baseAddress.GetHeap(), baseAddress.GetOffset() + m_offset);
	}

	inline MemoryPosition MemoryPosition::operator+(GPUMemoryOffset_t offset) const
	{
		RKIT_ASSERT(m_memAllocation != nullptr);
		RKIT_ASSERT(offset <= m_memAllocation->GetSize() - m_offset);
		return MemoryPosition(*m_memAllocation, offset);
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

		return MemoryRegion(MemoryPosition(*m_position.GetAllocation(), m_position.GetOffset() + offset), size);
	}

	inline const MemoryPosition &MemoryRegion::GetPosition() const
	{
		return m_position;
	}

	inline IMemoryAllocation *MemoryRegion::GetAllocation() const
	{
		return m_position.GetAllocation();
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

	inline MemoryRegion IMemoryAllocation::GetRegion() const
	{
		return MemoryRegion(MemoryPosition(*const_cast<IMemoryAllocation *>(this), 0), this->GetSize());
	}

	inline MemoryAddress IMemoryHeap::GetBaseAddress() const
	{
		return MemoryAddress(*const_cast<IMemoryHeap *>(this), 0);
	}
} }
