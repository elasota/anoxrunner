#pragma once

#include "rkit/Core/Module.h"
#include "rkit/Core/String.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct SystemModuleInitParameters_Win32 : public ModuleInitParameters
	{
		explicit SystemModuleInitParameters_Win32(const HINSTANCE &hinstance, Utf16String &&executablePath, Utf16String &&programDir)
			: m_hinst(hinstance)
			, m_executablePath(std::move(executablePath))
			, m_programDir(std::move(programDir))
		{
		}

		HINSTANCE m_hinst;
		Utf16String m_executablePath;
		Utf16String m_programDir;
	};
}
