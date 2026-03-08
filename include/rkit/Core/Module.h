#pragma once

#include "rkit/Core/Result.h"

namespace rkit
{
	struct Drivers;

	struct ModuleInitParameters
	{
	};

	struct IModule
	{
	public:
		// Custom drivers are not guaranteed to be supported!
		virtual Result InitWithCustomDrivers(const ModuleInitParameters *initParams, Drivers *drivers) = 0;
		virtual Result Init(const ModuleInitParameters *initParams) = 0;
		virtual void Unload() = 0;

	protected:
		virtual ~IModule() {};
	};
}
