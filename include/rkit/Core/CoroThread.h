#pragma once

#include "CoroutineProtos.h"
#include "Result.h"

#include <coroutine>

namespace rkit
{
	class FutureBase;
}

namespace rkit
{
	struct CoroThreadResumer
	{
		std::coroutine_handle<> m_continuation;
	};

	struct CoroThreadBlocker
	{
		typedef bool (*CheckUnblockFunc_t)(void *context);
		typedef rkit::Result(*ConsumeFunc_t)(void *context);
		typedef void (*ReleaseFunc_t)(void *context);

		void *m_context = nullptr;

		// Check if this can be resumed
		CheckUnblockFunc_t m_checkFunc = nullptr;

		// Consume the blocker and return any waiting result
		ConsumeFunc_t m_consumeFunc = nullptr;

		// Release the blocker without consuming its result
		ReleaseFunc_t m_releaseFunc = nullptr;

		bool m_autoReleaseOnResume = false;

		static CoroThreadBlocker Create(void *context, CheckUnblockFunc_t checkFunc, ConsumeFunc_t consumeFunc, ReleaseFunc_t releaseFunc, bool autoReleaseOnResume);
	};

	class CoroThreadBlockerAwaiter
	{
	public:
		CoroThreadBlockerAwaiter(CoroThreadBlocker *blocker, CoroThreadResumer *resumer);

		bool await_ready() const;
		void await_suspend(std::coroutine_handle<> continuation) const;
		rkit::Result await_resume();

	private:
		CoroThreadBlocker *m_blocker;
		CoroThreadResumer *m_resumer;

		CoroThreadBlockerAwaiter() = delete;
	};

	struct ICoroThread
	{
		virtual ~ICoroThread() {}

		virtual CoroThreadState GetState() const = 0;
		virtual CoroThreadBlockerAwaiter AwaitFuture(const FutureBase &future) = 0;

		virtual rkit::Result Resume() = 0;
		virtual bool TryUnblock() = 0;
		virtual void *AllocFrame(size_t size) = 0;
		virtual void DeallocFrame(void *mem) = 0;

		template<class TReturnType>
		rkit::Result EnterFunction(const coro::Coroutine<TReturnType> &coro);

	protected:
		virtual rkit::Result EnterCoroutine(std::coroutine_handle<> coroHandle) = 0;
	};
}

namespace rkit
{
	inline CoroThreadBlocker CoroThreadBlocker::Create(void *context, CheckUnblockFunc_t checkFunc, ConsumeFunc_t consumeFunc, ReleaseFunc_t releaseFunc, bool autoReleaseOnResume)
	{
		return CoroThreadBlocker{ context, checkFunc, consumeFunc, releaseFunc, autoReleaseOnResume };
	}

	inline CoroThreadBlockerAwaiter::CoroThreadBlockerAwaiter(CoroThreadBlocker *blocker, CoroThreadResumer *resumer)
		: m_blocker(blocker)
		, m_resumer(resumer)
	{
	}

	inline bool CoroThreadBlockerAwaiter::await_ready() const
	{
		return m_blocker->m_checkFunc(m_blocker->m_context);
	}

	inline void CoroThreadBlockerAwaiter::await_suspend(std::coroutine_handle<> continuation) const
	{
		m_resumer->m_continuation = continuation;
	}

	inline rkit::Result CoroThreadBlockerAwaiter::await_resume()
	{
		CoroThreadBlocker::ConsumeFunc_t consumeFunc = m_blocker->m_consumeFunc;
		void *context = m_blocker->m_context;

		(*m_blocker) = {};

		return consumeFunc(context);
	}

	template<class TReturnType>
	rkit::Result ICoroThread::EnterFunction(const coro::Coroutine<TReturnType> &coro)
	{
		if (!coro.GetCoroutineHandle())
			RKIT_THROW(ResultCode::kCoroStackOverflow);

		return this->EnterCoroutine(coro.GetCoroutineHandle());
	}

}
