#include "rkit/Core/Event.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/SystemDriver.h"

#include "Win32AsyncIOThread.h"

namespace rkit
{
	class AsyncIOThreadContext_Win32 final : public IThreadContext
	{
	public:
		AsyncIOThreadContext_Win32(IEvent &startAndTerminateEvent, IMutex &pendingQueueMutex, IMutex &inProgressQueueMutex, HANDLE threadKickEvent);

		Result Run() override;

		void PostTask(AsyncIOTaskItem_Win32 &task);
		void AddInProgressTask(AsyncIOTaskItem_Win32 &task);
		void RemoveInProgressTask(AsyncIOTaskItem_Win32 &task);

		AsyncIOTaskItem_Win32 *DequeueTask();

		void Close();

	private:
		IEvent &m_startAndTerminateEvent;
		IMutex &m_pendingQueueMutex;
		IMutex &m_inProgressQueueMutex;
		HANDLE m_threadKickEvent;
		bool m_isClosing = false;

		AsyncIOTaskItem_Win32 *m_firstPendingTask = nullptr;
		AsyncIOTaskItem_Win32 *m_lastPendingTask = nullptr;

		AsyncIOTaskItem_Win32 *m_firstInProgressTask = nullptr;
		AsyncIOTaskItem_Win32 *m_lastInProgressTask = nullptr;
	};


	AsyncIOThreadContext_Win32::AsyncIOThreadContext_Win32(IEvent &startAndTerminateEvent, IMutex &pendingQueueMutex, IMutex &inProgressQueueMutex, HANDLE threadKickEvent)
		: m_startAndTerminateEvent(startAndTerminateEvent)
		, m_pendingQueueMutex(pendingQueueMutex)
		, m_inProgressQueueMutex(inProgressQueueMutex)
		, m_threadKickEvent(threadKickEvent)
	{
	}

	Result AsyncIOThreadContext_Win32::Run()
	{
		m_startAndTerminateEvent.Signal();

		for (;;)
		{
			DWORD waitResult = WaitForMultipleObjectsEx(1, &m_threadKickEvent, FALSE, INFINITE, TRUE);
			if (waitResult == WAIT_IO_COMPLETION)
				continue;

			while (AsyncIOTaskItem_Win32 *task = DequeueTask())
				task->m_executeFunc(task->m_userdata);

			if (m_isClosing)
				break;
		}

		m_startAndTerminateEvent.Signal();

		return ResultCode::kOK;
	}

	void AsyncIOThreadContext_Win32::PostTask(AsyncIOTaskItem_Win32 &task)
	{
		RKIT_ASSERT(task.m_prev == nullptr && task.m_next == nullptr);

		MutexLock lock(m_pendingQueueMutex);
		task.m_prev = m_lastPendingTask;

		if (m_lastPendingTask)
			m_lastPendingTask->m_next = &task;
		else
			m_firstPendingTask = &task;

		m_lastPendingTask = &task;

		::SetEvent(m_threadKickEvent);
	}

	void AsyncIOThreadContext_Win32::AddInProgressTask(AsyncIOTaskItem_Win32 &task)
	{
		RKIT_ASSERT(task.m_prev == nullptr && task.m_next == nullptr);

		MutexLock lock(m_inProgressQueueMutex);
		task.m_prev = m_lastInProgressTask;

		if (m_lastInProgressTask)
			m_lastInProgressTask->m_next = &task;
		else
			m_firstInProgressTask = &task;

		m_lastInProgressTask = &task;
	}

	void AsyncIOThreadContext_Win32::RemoveInProgressTask(AsyncIOTaskItem_Win32 &task)
	{
		{
			MutexLock lock(m_inProgressQueueMutex);

			if (m_firstInProgressTask == &task)
				m_firstInProgressTask = task.m_next;

			if (m_lastInProgressTask == &task)
				m_lastInProgressTask = task.m_prev;

			if (task.m_prev)
				task.m_prev->m_next = task.m_next;
			if (task.m_next)
				task.m_next->m_prev = task.m_prev;
		}

		task.m_prev = nullptr;
		task.m_next = nullptr;
	}

	AsyncIOTaskItem_Win32 *AsyncIOThreadContext_Win32::DequeueTask()
	{
		AsyncIOTaskItem_Win32 *task = nullptr;

		{
			MutexLock lock(m_pendingQueueMutex);
			task = m_firstPendingTask;

			if (!task)
				return nullptr;

			m_firstPendingTask = task->m_next;
			if (m_firstPendingTask)
				m_firstPendingTask->m_prev = nullptr;
			else
				m_lastPendingTask = nullptr;
		}

		task->m_next = nullptr;
		task->m_prev = nullptr;

		return task;
	}

	void AsyncIOThreadContext_Win32::Close()
	{
		while (AsyncIOTaskItem_Win32 *taskItem = DequeueTask())
			taskItem->m_cancelFunc(taskItem->m_userdata);

		for (;;)
		{
			void *firstUserdata = nullptr;
			void (*firstFlushFunc)(void *);

			{
				MutexLock lock(m_inProgressQueueMutex);
				if (m_firstInProgressTask == nullptr)
					break;

				firstUserdata = m_firstInProgressTask->m_userdata;
				firstFlushFunc = m_firstInProgressTask->m_flushFunc;
			}

			firstFlushFunc(firstUserdata);

			::SleepEx(0, TRUE);
		}

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
			m_threadContext->Close();
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

		RKIT_CHECK(m_sysDriver.CreateMutex(m_pendingQueueMutex));
		RKIT_CHECK(m_sysDriver.CreateMutex(m_inProgressQueueMutex));

		RKIT_CHECK(m_sysDriver.CreateEvent(m_startAndTerminateEvent, true, false));

		UniquePtr<AsyncIOThreadContext_Win32> threadContext;
		RKIT_CHECK(New<AsyncIOThreadContext_Win32>(threadContext, *m_startAndTerminateEvent, *m_pendingQueueMutex, *m_inProgressQueueMutex, m_kickEvent));

		m_threadContext = threadContext.Get();

		RKIT_CHECK(m_sysDriver.CreateThread(m_thread, std::move(threadContext), u8"AsyncIO"));

		m_startAndTerminateEvent->Wait();

		return ResultCode::kOK;
	}

	void AsyncIOThread_Win32::PostTask(AsyncIOTaskItem_Win32 &task)
	{
		m_threadContext->PostTask(task);
	}

	void AsyncIOThread_Win32::PostInProgress(AsyncIOTaskItem_Win32 &task)
	{
		m_threadContext->AddInProgressTask(task);
	}

	void AsyncIOThread_Win32::RemoveInProgress(AsyncIOTaskItem_Win32 &task)
	{
		m_threadContext->RemoveInProgressTask(task);
	}
}
