#pragma once

#include <cstddef>

namespace rkit
{
	struct IMallocDriver
	{
		virtual ~IMallocDriver() {}

		virtual void *Alloc(size_t size) = 0;
		virtual void *Realloc(void *ptr, size_t size) = 0;
		virtual void Free(void *ptr) = 0;
	};
}
