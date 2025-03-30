#pragma once

#include "rkit/Core/Result.h"


#ifdef NDEBUG

#define RKIT_IMPLEMENT_ASSERTION_CHECK_FUNC

#else

#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Drivers.h"

#include <cstdlib>
#include <cassert>

#define RKIT_IMPLEMENT_ASSERTION_CHECK_FUNC	\
	void rkit::priv::AssertionCheckFunc(bool expr, const char *exprStr, const char *file, unsigned int line)\
	{\
		if (!expr)\
		{\
			ISystemDriver *systemDriver = GetDrivers().m_systemDriver;\
			if (systemDriver)\
				systemDriver->AssertionFailure(exprStr, file, line);\
			else\
				abort();\
		}\
	}

#endif


#if RKIT_IS_DEBUG

#define RKIT_IMPLEMENT_RESULT_FIRST_CHANCE_RESULT_FAILURE\
	void rkit::Result::FirstChanceResultFailure() const\
	{\
		::rkit::ISystemDriver *sysDriver = ::rkit::GetDrivers().m_systemDriver;\
		if (sysDriver) sysDriver->FirstChanceResultFailure(*this);\
	}

#else

#define RKIT_IMPLEMENT_RESULT_FIRST_CHANCE_RESULT_FAILURE

#endif

#define RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS	\
	RKIT_IMPLEMENT_RESULT_FIRST_CHANCE_RESULT_FAILURE\
	RKIT_IMPLEMENT_ASSERTION_CHECK_FUNC

#if defined(_WIN32)

#include "rkit/Win32/ModuleAPI_Win32.h"

#define RKIT_IMPLEMENT_MODULE(moduleNamespace, moduleName, moduleClass)	\
namespace rkit\
{\
	struct Drivers;\
	Drivers *g_drivers;\
	const Drivers &GetDrivers() { return *g_drivers; }\
	Drivers &GetMutableDrivers() { return *g_drivers; }\
}\
extern "C" __declspec(dllexport) void InitializeRKitModule(void *moduleAPIPtr)\
{\
	::rkit::ModuleAPI_Win32 *moduleAPI = static_cast<::rkit::ModuleAPI_Win32 *>(moduleAPIPtr);\
	::rkit::g_drivers = moduleAPI->m_drivers;\
	moduleAPI->m_initFunction = moduleClass::Init;\
	moduleAPI->m_shutdownFunction = moduleClass::Shutdown;\
}\
RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS


#endif
