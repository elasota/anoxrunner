#pragma once

#include "rkit/Core/Span.h"
#include "rkit/Sandbox/SandboxSysCall.h"
#include "rkit/Sandbox/SandboxAddress.h"

#include <stddef.h>

namespace rkit
{
	struct ISandbox;
}

namespace rkit::sandbox
{
	struct HostAPIName
	{
		const Utf8Char_t *m_name;
		size_t m_nameLength;
	};

	struct HostAPIDescriptor
	{
		rkit::ISandbox *m_sandbox = nullptr;

		SysCallCatalog m_sysCallCatalog;
		const HostAPIName *m_sysCallNames;

		Address_t *m_imports;
		const HostAPIName *m_importNames;
		size_t m_numImports;
	};
}
