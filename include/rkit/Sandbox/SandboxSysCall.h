#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Sandbox/SandboxAddress.h"

#include <stdint.h>

namespace rkit
{
	struct ISandbox;
}

namespace rkit::sandbox
{
	struct Environment;
	struct IThreadContext;
}

namespace rkit::sandbox::io
{
	typedef uint64_t Value_t;
	typedef void (*CriticalErrorFunc_t)(ISandbox *sandbox, Environment *env, IThreadContext *thread) noexcept;
	typedef PackedResultAndExtCode (*SysCallDispatchFunc_t)(ISandbox *sandbox, Environment *env, IThreadContext *thread, sandbox::Address_t ioValuesAddr) noexcept;
}

namespace rkit::sandbox
{
	struct SysCallCatalog
	{
		const ::rkit::sandbox::io::SysCallDispatchFunc_t *m_sysCalls;
		size_t m_numSysCalls;
	};
}
