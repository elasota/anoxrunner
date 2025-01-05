#include "rkit/Render/DisplayManager.h"

#include "IncludeWindows.h"

namespace rkit::render
{
	struct IDisplay_Win32 : public IDisplay
	{
		virtual HWND GetHWND() = 0;
		virtual HINSTANCE GetHINSTANCE() = 0;
	};
}
