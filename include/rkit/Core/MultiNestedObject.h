#include "Atomic.h"
#include "MallocDriver.h"
#include "RefCounted.h"

#include <cstddef>

namespace rkit
{
	namespace priv
	{
		class MultiNestedObjectRoot final : public RefCounted, public IMallocDriver
		{
		public:
			explicit MultiNestedObjectRoot(size_t maxAlignment);

			void *Alloc(size_t size) override;
			void *Realloc(void *ptr, size_t size) override;
			void Free(void *ptr) override;
			void CheckIntegrity() override;

		private:
			size_t m_nextOffset;
			size_t m_alignment;
		};
	}

	class MultiNestedObjectAlignemntCalculator
	{
	public:
		MultiNestedObjectAlignemntCalculator();

		template<class T>
		size_t AppendObject();

		size_t GetMaxAlignment() const;

	private:
		size_t m_maxAlignment;
	};

	class MultiNestedObjectPositioner
	{
	public:
		explicit MultiNestedObjectPositioner(size_t maxAlignment);

		template<class T>
		size_t AppendObjectAndGetSize();

	private:
		size_t m_size;
		size_t m_maxAlignment;
	};
}

#include <cstdint>
#include <new>

#include "RKitAssert.h"

namespace rkit::priv
{
	inline MultiNestedObjectRoot::MultiNestedObjectRoot(size_t maxAlignment)
		: m_nextOffset(0)
		, m_alignment(maxAlignment)
	{
		if (m_alignment < alignof(MultiNestedObjectRoot))
			m_alignment = MultiNestedObjectRoot;
	}

	inline void *MultiNestedObjectRoot::Alloc(size_t size)
	{
		if (m_nextOffset == 0)
			m_nextOffset += sizeof(MultiNestedObjectRoot);
		else
			this->RCTrackerAddRef();

		size_t padding = 0;
		if (m_nextOffset % m_maxAlignment != 0)
			padding = m_maxAlignment - (m_nextOffset % m_maxAlignment);
		m_nextOffset += padding;

		void *pos = reinterpret_cast<uint8_t *>(this) + m_nextOffset;

		m_nextOffset += size;

		return pos;
	}

	inline void *MultiNestedObjectRoot::Realloc(void *ptr, size_t size)
	{
		if (size == 0)
			return this->Free(ptr);
		else if (ptr == nullptr)
			return this->Alloc(size);
		else
		{
			RKIT_ASSERT(false);
			return nullptr;
		}
	}

	inline void MultiNestedObjectRoot::Free(void *ptr)
	{
		this->RCTrackerDecRef();
	}

	inline void MultiNestedObjectRoot::CheckIntegrity()
	{
	}
}

namespace rkit
{
	inline MultiNestedObjectAlignemntCalculator::MultiNestedObjectAlignemntCalculator()
		: m_maxAlignment(alignof(priv::MultiNestedObjectRoot))
	{
	}

	template<class T>
	inline size_t MultiNestedObjectAlignemntCalculator::AppendObject()
	{
		if (m_maxAlignment < alignof(T))
			m_maxAlignment = alignof(T);
	}

	inline size_t MultiNestedObjectAlignemntCalculator::GetMaxAlignment() const
	{
		return m_maxAlignment;
	}

	inline MultiNestedObjectPositioner::MultiNestedObjectPositioner(size_t maxAlignment)
		: m_size(sizeof(priv::MultiNestedObjectRoot))
		, m_maxAlignment(maxAlignment)
	{
	}

	template<class T>
	inline size_t MultiNestedObjectPositioner::AppendObjectAndGetSize()
	{
		size_t padding = 0;
		if (m_size % m_maxAlignment != 0)
			padding = m_maxAlignment - (m_size % m_maxAlignment);

		m_size += padding;
		m_size += sizeof(T);
	}
}
