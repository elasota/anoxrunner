#include "rkit/Render/DisplayManager.h"

#include "IncludeWindows.h"

namespace rkit::render
{
	struct IDisplay_Win32 : public IDisplay
	{
		virtual Result PostWindowThreadTask(void *userdata, void (*callback)(void *userdata)) = 0;

		virtual HWND GetHWND() = 0;
		virtual HINSTANCE GetHINSTANCE() = 0;
	};
}
