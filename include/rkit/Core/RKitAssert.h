#pragma once

#ifdef NDEBUG
#define RKIT_ASSERT(n) ((void)0)
#else

namespace rkit
{
	namespace Private
	{
		inline void AssertionCheckFunc(bool expr, const char *exprStr, const char *file, unsigned int line);
	}
}

#define RKIT_ASSERT(expr)	(::rkit::Private::AssertionCheckFunc(!!(expr), #expr, __FILE__, __LINE__))

#endif


#ifdef NDEBUG
#else

#include "SystemDriver.h"
#include "CoreDefs.h"
#include "Drivers.h"

#include <cstdlib>
#include <assert.h>

inline void rkit::Private::AssertionCheckFunc(bool expr, const char *exprStr, const char *file, unsigned int line)
{
	if (!expr)
	{
		ISystemDriver *systemDriver = GetDrivers().m_systemDriver;
		if (systemDriver)
			systemDriver->AssertionFailure(exprStr, file, line);
		else
			abort();
	}
}
#endif
