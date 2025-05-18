#pragma once

#include "rkit/Core/NoCopy.h"

#include "rkit/Render/DisplayManager.h"

#include "rkit/Win32/IncludeWindows.h"

namespace rkit
{
	struct IMallocDriver;
}

namespace rkit { namespace render
{
	class DisplayManagerBase_Win32 : public IDisplayManager, public NoCopy
	{
	public:
		static Result Create(UniquePtr<DisplayManagerBase_Win32> &outDisplayManager, IMallocDriver *alloc, HINSTANCE hInst);
	};
} } // rkit::render
