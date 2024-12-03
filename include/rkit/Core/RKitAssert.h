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

