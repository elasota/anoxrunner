#pragma once

#ifdef NDEBUG
#define RKIT_ASSERTS_ONLY(n)
#define RKIT_ASSERT(n) ((void)0)
#define RKIT_VERIFY(n)	((void)!!(n))

#define RKIT_ASSERT_WITH_DRIVER(expr, driver)	RKIT_ASSERT(expr)
#define RKIT_VERIFY_WITH_DRIVER(expr, driver)	RKIT_VERIFY(expr)

#else

namespace rkit
{
	struct IAssertDriver
	{
		virtual void AssertionFailure(const char *expr, const char *file, unsigned int line) = 0;
	};
}

namespace rkit::priv
{
	void AssertionCheckFunc(bool expr, const char *exprStr, const char *file, unsigned int line);

	inline void AssertionCheckWithSystemDriverFunc(bool expr, IAssertDriver *assertDriver, const char *exprStr, const char *file, unsigned int line)
	{
		if (!expr)
			assertDriver->AssertionFailure(exprStr, file, line);
	}
}

#define RKIT_ASSERTS_ONLY(n) n

#define RKIT_ASSERT_WITH_DRIVER(expr, driver)	(::rkit::priv::AssertionCheckWithSystemDriverFunc(!!(expr), (driver), #expr, __FILE__, __LINE__))
#define RKIT_VERIFY_WITH_DRIVER(expr, driver)	(::rkit::priv::AssertionCheckWithSystemDriverFunc(!!(expr), (driver), #expr, __FILE__, __LINE__))

#define RKIT_ASSERT(expr)	(::rkit::priv::AssertionCheckFunc(!!(expr), #expr, __FILE__, __LINE__))
#define RKIT_VERIFY(expr)	(::rkit::priv::AssertionCheckFunc(!!(expr), #expr, __FILE__, __LINE__))

#endif

