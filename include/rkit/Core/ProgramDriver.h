#pragma once

#include "Drivers.h"
#include "Result.h"


namespace rkit
{
	struct Result;

	struct IProgramDriver
	{
		virtual ~IProgramDriver() {}

		virtual Result InitProgram() = 0;
		virtual Result RunFrame(bool &outIsExiting) = 0;
		virtual void ShutdownProgram() = 0;
	};
}
