#pragma once

#include "rkit/Core/Module.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct SystemModuleInitParameters_Win32 : public ModuleInitParameters
	{
		explicit SystemModuleInitParameters_Win32(const HINSTANCE &hinstance)
			: m_hinst(hinstance)
		{
		}

		HINSTANCE m_hinst;
	};
}
