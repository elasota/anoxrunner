#pragma once

#include "rkit/Core/SystemDriver.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct IWin32PlatformDriver : public IPlatformDriver
	{
		virtual HINSTANCE GetHInstance() const = 0;
	};
}
