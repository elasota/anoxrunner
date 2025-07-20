#pragma once

#include "rkit/Core/Module.h"
#include "rkit/Core/String.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct SystemModuleInitParameters_Win32 : public ModuleInitParameters
	{
		explicit SystemModuleInitParameters_Win32(const HINSTANCE &hinstance, WString16 &&executablePath, WString16 &&programDir)
			: m_hinst(hinstance)
			, m_executablePath(std::move(executablePath))
			, m_programDir(std::move(programDir))
		{
		}

		HINSTANCE m_hinst;
		WString16 m_executablePath;
		WString16 m_programDir;
	};
}
