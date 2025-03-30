#pragma once

#include "rkit/Core/Module.h"
#include "rkit/Core/String.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct SystemModuleInitParameters_Win32 : public ModuleInitParameters
	{
		explicit SystemModuleInitParameters_Win32(const HINSTANCE &hinstance, WString &&executablePath, WString &&programDir)
			: m_hinst(hinstance)
			, m_executablePath(std::move(executablePath))
			, m_programDir(std::move(programDir))
		{
		}

		HINSTANCE m_hinst;
		WString m_executablePath;
		WString m_programDir;
	};
}
