#include "rkit/Core/Event.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/SystemDriver.h"


#include "Win32AsyncIOThread.h"

namespace rkit
{
	class AsyncIOThreadContext_Win32 final : public IThreadContext
	{
	public:
		AsyncIOThreadContext_Win32(IEvent &startAndTerminateEvent, HANDLE threadKickEvent);

		Result Run() override;

		void SignalClose();

	private:
		IEvent &m_startAndTerminateEvent;
		HANDLE m_threadKickEvent;
		bool m_isClosing = false;
	};


	AsyncIOThreadContext_Win32::AsyncIOThreadContext_Win32(IEvent &startAndTerminateEvent, HANDLE threadKickEvent)
		: m_startAndTerminateEvent(startAndTerminateEvent)
		, m_threadKickEvent(threadKickEvent)
	{
	}

	Result AsyncIOThreadContext_Win32::Run()
	{
		m_startAndTerminateEvent.Signal();

		for (;;)
		{
			DWORD waitResult = WaitForMultipleObjectsEx(1, &m_threadKickEvent, TRUE, INFINITE, TRUE);
			if (waitResult == WAIT_IO_COMPLETION)
				continue;

			if (m_isClosing)
				break;
		}

		m_startAndTerminateEvent.Signal();

		return ResultCode::kOK;
	}

	void AsyncIOThreadContext_Win32::SignalClose()
	{
		m_isClosing = true;
		::SetEvent(m_threadKickEvent);
	}

	AsyncIOThread_Win32::AsyncIOThread_Win32(ISystemDriver &sysDriver)
		: m_sysDriver(sysDriver)
		, m_kickEvent(nullptr)
	{
	}

	AsyncIOThread_Win32::~AsyncIOThread_Win32()
	{
		if (m_thread.IsValid())
		{
			m_threadContext->SignalClose();
			::SetEvent(m_kickEvent);
			(void) m_thread.Finalize();
		}

		if (m_kickEvent)
			::CloseHandle(m_kickEvent);
	}

	Result AsyncIOThread_Win32::Initialize()
	{
		m_kickEvent = ::CreateEventA(nullptr, FALSE, FALSE, "AsyncIOThreadKick");
		if (!m_kickEvent)
			return ResultCode::kOperationFailed;

		RKIT_CHECK(m_sysDriver.CreateEvent(m_startAndTerminateEvent, true, false));

		UniquePtr<AsyncIOThreadContext_Win32> threadContext;
		RKIT_CHECK(New<AsyncIOThreadContext_Win32>(threadContext, *m_startAndTerminateEvent, m_kickEvent));

		m_threadContext = threadContext.Get();

		RKIT_CHECK(m_sysDriver.CreateThread(m_thread, std::move(threadContext)));

		m_startAndTerminateEvent->Wait();

		return ResultCode::kOK;
	}
}
