#pragma once

#include "rkit/Core/Module.h"
#include "rkit/Sandbox/SandboxAddress.h"
#include "rkit/Sandbox/SandboxIO.h"

namespace rkit
{
	struct ISandbox;
}

namespace rkit::sandbox
{
	struct Environment;
}

namespace rkit::sandbox
{
	struct SandboxModuleInitParams : public ModuleInitParameters
	{
		Address_t *m_outEntryDescriptor = nullptr;
		ISandbox ***m_outSandboxPtr = nullptr;
		Environment *m_envPtr = nullptr;
		const io::SysCallDispatchFunc_t *m_sysCalls = nullptr;
		io::CriticalErrorFunc_t m_criticalError = nullptr;
	};
}
