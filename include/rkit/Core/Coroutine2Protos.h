#pragma once

#include "Result.h"

namespace rkit::coro2
{
	template<class TReturnType>
	class Coroutine;
}

namespace rkit::coro2::priv
{
	template<class TReturnType>
	class Promise;
}

namespace rkit
{
	struct ICoroThread;

	using ResultCoroutine = rkit::coro2::Coroutine<rkit::Result>;

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
