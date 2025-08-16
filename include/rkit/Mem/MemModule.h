#pragma once

#include "rkit/Core/Module.h"

namespace rkit { namespace mem {
	struct IMemMapDriver;

	struct MemModuleInitParameters : public ModuleInitParameters
	{
		IMemMapDriver *m_mmapDriver;
	};
} } // rkit::mem
