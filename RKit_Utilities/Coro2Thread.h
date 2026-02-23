#pragma once

#include "rkit/Core/Coroutine.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	class Coro2ThreadBase : public ICoroThread
	{
	public:
		static Result Create(UniquePtr<Coro2ThreadBase> &coroThread, size_t stackSize);
	};
} // rkit::utils
