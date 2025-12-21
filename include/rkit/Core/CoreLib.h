#pragma once

#include "rkit/Core/CoreDefs.h"

#if RKIT_CORELIB_DLL != 0
#	ifdef RKIT_CORELIB_INTERNAL
#		define RKIT_CORELIB_API RKIT_DLLEXPORT_API
#	else
#		define RKIT_CORELIB_API RKIT_DLLIMPORT_API
#	endif
#else
#	define RKIT_CORELIB_API
#endif
