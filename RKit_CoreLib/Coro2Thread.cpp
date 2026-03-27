#include "Coro2Thread.h"

#include "rkit/Core/RKitAssert.h"
#include "rkit/Core/CoroFinalizer.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/NewDelete.h"

#include "rkit/Core/CoreLib.h"

namespace rkit::utils
{
	class FutureBlocker
	{
	public:
		static CoroThreadBlocker Create(const FutureBase &future);

	private:
		static bool CheckUnblock(void *context);
		static rkit::Result Consume(void *context);
		static void Release(void *context);
	};

	class NotActuallyBlockedBlocker
	{
	public:
		static CoroThreadBlocker Create();

	private:
		static bool CheckUnblock(void *context);
		static rkit::Result Consume(void *context);
		static void Release(void *context);
	};

	struct Coro2StackFrame
	{
		size_t m_nextFrameOffset;

		// Low bit is 1 if the frame is free
		static const size_t kAlignment = (__STDCPP_DEFAULT_NEW_ALIGNMENT__ < 2) ? 2 : __STDCPP_DEFAULT_NEW_ALIGNMENT__;

		static size_t ComputeBaseSize();
	};

	class Coro2Thread final: public Coro2ThreadBase
	{
	public:
		explicit Coro2Thread(size_t stackSize
#ifndef NDEBUG
			, IAssertDriver *assertDriver
#endif
		);
		~Coro2Thread();

		CoroThreadState GetState() const override;
		CoroThreadBlockerAwaiter AwaitFuture(const FutureBase &future) override;
		CoroThreadBlockerAwaiter AwaitBlocker(const CoroThreadBlocker &blocker) override;

		rkit::Result Resume() override;
		bool TryUnblock() override;
		void *AllocFrame(size_t size) override;
		void DeallocFrame(void *mem) override;

		static size_t ComputeBaseSize();

	protected:
		rkit::Result EnterCoroutine(std::coroutine_handle<> coroHandle, const coro::CoroFinalizer &finalizer) override;

	private:
		CoroThreadBlocker m_blocker;
		CoroThreadResumer m_resumer;
		std::coroutine_handle<> m_rootFunction;
		coro::CoroFinalizer m_finalizer = {};

		uint8_t *GetStackStart();

		uint8_t *m_stackEnd;
		uint8_t *m_stackTopFrame;

		RKIT_ASSERTS_ONLY(IAssertDriver *m_assertDriver;)
	};

	CoroThreadBlocker FutureBlocker::Create(const FutureBase &future)
	{
		FutureContainerBase *baseRC = future.GetFutureContainerRCPtr().Get();

		baseRC->RCIncRef();

		return CoroThreadBlocker::Create(baseRC, CheckUnblock, Consume, Release, false);
	}

	bool FutureBlocker::CheckUnblock(void *context)
	{
		FutureContainerBase *container = static_cast<FutureContainerBase *>(context);

		switch (container->GetState())
		{
		case FutureState::kFailed:
		case FutureState::kAborted:
		case FutureState::kCompleted:
			return true;
		default:
			return false;
		}
	}

	rkit::Result FutureBlocker::Consume(void *context)
	{
		Release(context);
		RKIT_RETURN_OK;
	}

	void FutureBlocker::Release(void *context)
	{
		static_cast<FutureContainerBase *>(context)->RCDecRef();
	}

	CoroThreadBlocker NotActuallyBlockedBlocker::Create()
	{
		return CoroThreadBlocker::Create(nullptr, CheckUnblock, Consume, Release, true);
	}

	bool NotActuallyBlockedBlocker::CheckUnblock(void *context)
	{
		return true;
	}

	rkit::Result NotActuallyBlockedBlocker::Consume(void *context)
	{
		RKIT_RETURN_OK;
	}

	void NotActuallyBlockedBlocker::Release(void *context)
	{
	}

	size_t Coro2StackFrame::ComputeBaseSize()
	{
		return rkit::AlignUp<size_t>(sizeof(Coro2StackFrame), kAlignment);
	}

	Coro2Thread::Coro2Thread(size_t stackSize
#ifndef NDEBUG
		, IAssertDriver *assertDriver
#endif
	)
		: m_stackEnd(GetStackStart() + stackSize)
		, m_stackTopFrame(m_stackEnd)
#ifndef NDEBUG
		, m_assertDriver(assertDriver)
#endif
	{
	}

	Coro2Thread::~Coro2Thread()
	{
		if (m_blocker.m_releaseFunc)
			m_blocker.m_releaseFunc(m_blocker.m_context);

		if (m_resumer.m_continuation)
			m_resumer.m_continuation.destroy();

		// The root function should be automatically destroyed by destroying the continuation
	}

	CoroThreadState Coro2Thread::GetState() const
	{
		if (!m_resumer.m_continuation)
			return CoroThreadState::kInactive;

		if (m_blocker.m_checkFunc(m_blocker.m_context))
			return CoroThreadState::kSuspended;

		return CoroThreadState::kBlocked;
	}

	CoroThreadBlockerAwaiter Coro2Thread::AwaitFuture(const FutureBase &future)
	{
		return AwaitBlocker(FutureBlocker::Create(future));
	}

	CoroThreadBlockerAwaiter Coro2Thread::AwaitBlocker(const CoroThreadBlocker &blocker)
	{
		RKIT_ASSERT_WITH_DRIVER(m_blocker.m_context == nullptr, m_assertDriver);
		m_blocker = blocker;
		return CoroThreadBlockerAwaiter(&m_blocker, &m_resumer);
	}

	rkit::Result Coro2Thread::Resume()
	{
		RKIT_ASSERT_WITH_DRIVER(!!m_resumer.m_continuation, m_assertDriver);
		RKIT_ASSERT_WITH_DRIVER(GetState() == CoroThreadState::kSuspended, m_assertDriver);

		if (m_blocker.m_autoReleaseOnResume)
		{
			m_blocker.m_releaseFunc(m_blocker.m_context);
			m_blocker = CoroThreadBlocker();
		}

		std::coroutine_handle<> continuation = m_resumer.m_continuation;
		m_resumer.m_continuation = std::coroutine_handle<>();

		continuation.resume();

		if (m_rootFunction.done())
		{
			RKIT_ASSERT_WITH_DRIVER(!m_resumer.m_continuation, m_assertDriver);
			std::coroutine_handle<> coroHandle = m_rootFunction;
			m_rootFunction = std::coroutine_handle<>();

			RKIT_CHECK(m_finalizer.m_destroyAndRethrow(coroHandle, m_finalizer.m_context));
		}

		RKIT_RETURN_OK;
	}

	bool Coro2Thread::TryUnblock()
	{
		return m_blocker.m_checkFunc(m_blocker.m_context);
	}

	void *Coro2Thread::AllocFrame(size_t size)
	{
		const size_t initialAvailable = m_stackTopFrame - GetStackStart();
		size_t available = initialAvailable;

		size_t headerSize = Coro2StackFrame::ComputeBaseSize();

		if (available < headerSize)
			return nullptr;

		available -= headerSize;

		if (available < size)
			return nullptr;

		available -= size;
		available -= available % Coro2StackFrame::kAlignment;

		uint8_t *frameStart = GetStackStart() + available;

		m_stackTopFrame = frameStart;

		Coro2StackFrame *topFrame = reinterpret_cast<Coro2StackFrame *>(frameStart);
		topFrame->m_nextFrameOffset = (initialAvailable - available);

		return frameStart + headerSize;
	}

	void Coro2Thread::DeallocFrame(void *mem)
	{
		uint8_t *headerPos = static_cast<uint8_t *>(mem) - Coro2StackFrame::ComputeBaseSize();

		if (headerPos != m_stackTopFrame)
		{
			// Create a hole in the stack
			Coro2StackFrame *frame = reinterpret_cast<Coro2StackFrame *>(headerPos);
			frame->m_nextFrameOffset |= 1;
		}
		else
		{
			// Unwind stack frames
			Coro2StackFrame *topFrame = reinterpret_cast<Coro2StackFrame *>(headerPos);
			headerPos += topFrame->m_nextFrameOffset;

			while (headerPos != m_stackEnd)
			{
				Coro2StackFrame *frame = reinterpret_cast<Coro2StackFrame *>(headerPos);

				size_t nextOffset = frame->m_nextFrameOffset;

				// Is this a hole?
				if ((nextOffset & 1) == 0)
				{
					// No (it's in use)
					break;
				}

				// Yes, pop it off
				nextOffset -= (nextOffset & 1);
				headerPos += nextOffset;
			}

			m_stackTopFrame = headerPos;
		}
	}

	size_t Coro2Thread::ComputeBaseSize()
	{
		return rkit::AlignUp<size_t>(sizeof(Coro2Thread), Coro2StackFrame::kAlignment);
	}

	rkit::Result Coro2Thread::EnterCoroutine(std::coroutine_handle<> coroHandle, const coro::CoroFinalizer &finalizer)
	{
		RKIT_ASSERT_WITH_DRIVER(!m_resumer.m_continuation, m_assertDriver);
		RKIT_ASSERT_WITH_DRIVER(m_blocker.m_checkFunc == nullptr, m_assertDriver);

		m_resumer.m_continuation = coroHandle;

		m_blocker = NotActuallyBlockedBlocker::Create();
		m_rootFunction = coroHandle;
		m_finalizer = finalizer;

		RKIT_RETURN_OK;
	}


	uint8_t *Coro2Thread::GetStackStart()
	{
		return reinterpret_cast<uint8_t *>(this) + ComputeBaseSize();
	}

	Result Coro2ThreadBase::Create(UniquePtr<Coro2ThreadBase> &outThread, rkit::IMallocDriver *alloc, size_t stackSize
#ifdef NDEBUG
		, nullptr_t assertDriver
#else
		, IAssertDriver *assertDriver
#endif
		)
	{
		const size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

		const size_t threadObjectSize = Coro2Thread::ComputeBaseSize();

		{
			const size_t available = std::numeric_limits<size_t>::max() - threadObjectSize;
			if (available < stackSize)
				RKIT_THROW(ResultCode::kOutOfMemory);
		}

		const size_t requiredSize = threadObjectSize + stackSize;

		void *mem = alloc->Alloc(requiredSize);
		if (!mem)
			RKIT_THROW(ResultCode::kOutOfMemory);

		Coro2Thread *thread = new (mem) Coro2Thread(stackSize
#ifndef NDEBUG
			, assertDriver
#endif
		);

		outThread = UniquePtr<Coro2ThreadBase>(thread, mem, alloc);

		RKIT_RETURN_OK;
	}


	Result RKIT_CORELIB_API CreateCoroThread(UniquePtr<ICoroThread> &outThread, IMallocDriver *alloc, size_t stackSize
#ifdef NDEBUG
		, nullptr_t assertDriver
#else
		, IAssertDriver *assertDriver
#endif
	)
	{
		rkit::UniquePtr<Coro2ThreadBase> thread;
		RKIT_CHECK(Coro2ThreadBase::Create(thread, alloc, stackSize, assertDriver));
		outThread = std::move(thread);

		RKIT_RETURN_OK;
	}
}
