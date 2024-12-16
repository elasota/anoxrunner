#pragma once

#include "Module.h"

namespace rkit
{
	struct DriverInitParameters;

	struct DriverModuleInitParameters : public ModuleInitParameters
	{
		const DriverInitParameters *m_driverInitParams;
	};
}
