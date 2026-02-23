#pragma once

#include "Result.h"

namespace rkit::coro
{
	template<class TReturnType>
	class Coroutine;
}

namespace rkit::coro::priv
{
	template<class TReturnType>
	class Promise;
}

namespace rkit
{
	struct ICoroThread;

	using ResultCoroutine = rkit::coro::Coroutine<rkit::Result>;

	struct CoroThreadResumer;
	struct CoroThreadBlocker;
	class CoroThreadBlockerAwaiter;

	enum class CoroThreadState
	{
		kInactive,
		kSuspended,
		kBlocked,
	};
}
