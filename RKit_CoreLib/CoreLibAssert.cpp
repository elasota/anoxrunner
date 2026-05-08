#include "rkit/Core/RKitAssert.h"

#include "rkit/Core/CoreLib.h"

#if RKIT_CORELIB_DLL != 0 && defined(RKIT_CORELIB_INTERNAL)

namespace rkit::priv
{
	void AssertionCheckFunc(bool expr, const char *exprStr, const char *file, unsigned int line)
	{
	}
}

#endif
