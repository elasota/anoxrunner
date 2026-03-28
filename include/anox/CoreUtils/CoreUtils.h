#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/CoreLib.h"

#if RKIT_CORELIB_DLL != 0
#	ifdef ANOX_COREUTILS_INTERNAL
#		define ANOX_COREUTILS_API RKIT_DLLEXPORT_API
#	else
#		define ANOX_COREUTILS_API RKIT_DLLIMPORT_API
#	endif
#else
#	define ANOX_COREUTILS_API
#endif

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox::data
{
	struct EntityDefsSchema;
	struct EntityDefsSchema2;
}

namespace anox::utils
{
	const data::EntityDefsSchema ANOX_COREUTILS_API &GetEntityDefs();
}
