#pragma once

#include "rkit/Core/Coroutine.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	class CoroThreadBase : public coro::Thread
	{
	public:
		static Result Create(UniquePtr<CoroThreadBase> &coroThread, size_t stackSize);
	};
}
