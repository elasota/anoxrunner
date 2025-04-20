#pragma once

#include "Drivers.h"
#include "Result.h"


namespace rkit
{
	struct DriverInitParameters;

	struct IProgramDriver
	{
		virtual ~IProgramDriver() {}

		virtual rkit::Result InitDriver(const DriverInitParameters *) = 0;
		virtual void ShutdownDriver() = 0;

		virtual Result InitProgram() = 0;
		virtual Result RunFrame(bool &outIsExiting) = 0;
		virtual void ShutdownProgram() = 0;
	};
}
