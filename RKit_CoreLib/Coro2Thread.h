#pragma once

#include "rkit/Core/Coroutine.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IMallocDriver;

#ifndef NDEBUG
	struct IAssertDriver;
#endif
}

namespace rkit::utils
{
	class Coro2ThreadBase : public ICoroThread
	{
	public:
		static Result Create(UniquePtr<Coro2ThreadBase> &coroThread, IMallocDriver *alloc, size_t stackSize
#ifdef NDEBUG
			, nullptr_t assertDriver
#else
			, IAssertDriver *assertDriver
#endif
		);
	};
} // rkit::utils
