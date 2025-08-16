#pragma once

#include <cstddef>

namespace rkit
{
	struct IMallocDriver
	{
		virtual ~IMallocDriver() {}

		void *Alloc(size_t size);
		void *Realloc(void *ptr, size_t size);
		void Free(void *ptr);
		virtual bool TryResizeMemBlock(void *ptr, size_t newSize) { return false; }
		virtual void CheckIntegrity() {}

	protected:
		virtual void *InternalAlloc(size_t size) = 0;
		virtual void *InternalRealloc(void *ptr, size_t size) = 0;
		virtual void InternalFree(void *ptr) = 0;
	};

	inline void *IMallocDriver::Alloc(size_t size)
	{
		return (size == 0) ? nullptr : (this->InternalAlloc(size));
	}

	inline void *IMallocDriver::Realloc(void *ptr, size_t size)
	{
		if (ptr == nullptr)
			return this->Alloc(size);

		if (size == 0)
		{
			this->InternalFree(ptr);
			return nullptr;
		}
		return (size == 0) ? nullptr : (this->InternalRealloc(ptr, size));
	}

	inline void IMallocDriver::Free(void *ptr)
	{
		if (ptr != nullptr)
			this->InternalFree(ptr);
	}
}
