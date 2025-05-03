#pragma once

#include "rkit/Core/Thread.h"
#include "rkit/Core/UniquePtr.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct Result;
	struct IEvent;
	struct ISystemDriver;

	class AsyncIOThreadContext_Win32;

	class AsyncIOThread_Win32 final
	{
	public:
		explicit AsyncIOThread_Win32(ISystemDriver &sysDriver);
		~AsyncIOThread_Win32();

		Result Initialize();

	private:
		ISystemDriver &m_sysDriver;
		HANDLE m_kickEvent;
		UniquePtr<IEvent> m_startAndTerminateEvent;
		UniqueThreadRef m_thread;
		AsyncIOThreadContext_Win32 *m_threadContext = nullptr;
	};
}
