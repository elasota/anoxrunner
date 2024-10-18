#pragma once

namespace rkit
{
	struct Drivers;
	struct ModuleInitParameters;
	struct Result;

	struct ModuleAPI_Win32
	{
		Drivers *m_drivers;

		Result (*m_initFunction)(const ModuleInitParameters *initParams);
		void (*m_shutdownFunction)();
	};
}
