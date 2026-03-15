#pragma once

#include "rkit/Core/Result.h"

#include <coroutine>

namespace rkit::coro
{
	struct CoroFinalizer
	{
		rkit::Result (*m_destroyAndRethrow)(std::coroutine_handle<> coroHandle, void *context);
		void *m_context;
	};
}
