#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Thread.h"
#include "rkit/Core/UniquePtr.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct IEvent;
	struct ISystemDriver;

	class AsyncIOThreadContext_Win32;

	struct AsyncIOTaskItem_Win32
	{
		AsyncIOTaskItem_Win32 *m_prev = nullptr;
		AsyncIOTaskItem_Win32 *m_next = nullptr;

		void *m_userdata = nullptr;
		void (*m_executeFunc)(void *userdata);
		void (*m_cancelFunc)(void *userdata);
		void (*m_flushFunc)(void *userdata);
	};

	class AsyncIOThread_Win32 final
	{
	public:
		explicit AsyncIOThread_Win32(ISystemDriver &sysDriver);
		~AsyncIOThread_Win32();

		Result Initialize();

		void PostTask(AsyncIOTaskItem_Win32 &task);
		void PostInProgress(AsyncIOTaskItem_Win32 &task);
		void RemoveInProgress(AsyncIOTaskItem_Win32 &task);

	private:
		ISystemDriver &m_sysDriver;
		HANDLE m_kickEvent;
		UniquePtr<IMutex> m_pendingQueueMutex;
		UniquePtr<IMutex> m_inProgressQueueMutex;
		UniquePtr<IEvent> m_startAndTerminateEvent;
		UniqueThreadRef m_thread;
		AsyncIOThreadContext_Win32 *m_threadContext = nullptr;
	};
}
