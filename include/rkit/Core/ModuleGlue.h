#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/FourCC.h"

#define RKIT_MODULE_LINKER_TYPE_DLL	0
#define RKIT_MODULE_LINKER_TYPE_STATIC	1

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


#if RKIT_IS_DEBUG != 0 && (RKIT_RESULT_BEHAVIOR != RKIT_RESULT_BEHAVIOR_EXCEPTION)

#define RKIT_IMPLEMENT_RESULT_FIRST_CHANCE_RESULT_FAILURE	\
	void rkit::utils::FirstChanceResultFailure(PackedResultAndExtCode packedResult)\
	{\
		::rkit::ISystemDriver *sysDriver = ::rkit::GetDrivers().m_systemDriver; \
		if (sysDriver) sysDriver->FirstChanceResultFailure(packedResult); \
	}

#else

#define RKIT_IMPLEMENT_RESULT_FIRST_CHANCE_RESULT_FAILURE

#endif

#define RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS	\
	RKIT_IMPLEMENT_RESULT_FIRST_CHANCE_RESULT_FAILURE\
	RKIT_IMPLEMENT_ASSERTION_CHECK_FUNC

#if defined(_WIN32)

#include "rkit/Win32/ModuleAPI_Win32.h"

#if RKIT_MODULE_LINKER_TYPE == RKIT_MODULE_LINKER_TYPE_DLL

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

#elif RKIT_MODULE_LINKER_TYPE == RKIT_MODULE_LINKER_TYPE_STATIC

namespace rkit::moduleloader
{
	struct ModuleInfo
	{
		uint32_t m_namespace;
		const Utf8Char_t *m_name;
		size_t m_nameLength;

		rkit::Result(*m_initFunction)(const rkit::ModuleInitParameters *initParams);
		void (*m_shutdownFunction)();
	};

	struct ModuleList
	{
		ModuleInfo const* const* m_modules;
		size_t m_numModules;
	};

	constexpr uint32_t ComputeModuleNamespace(const char *moduleNamespace)
	{
		return rkit::utils::ComputeFourCC(moduleNamespace[0], moduleNamespace[1], moduleNamespace[2], moduleNamespace[3]);
	}

	extern const ModuleList g_moduleList;
}

#define RKIT_IMPLEMENT_MODULE(moduleNamespace, moduleName, moduleClass)	\
namespace rkit::moduleloader\
{\
	extern const ModuleInfo g_module_ ## moduleNamespace ## _ ## moduleName =\
	{\
		ComputeModuleNamespace(#moduleNamespace),\
		u8 ## #moduleName,\
		sizeof(u8 ## #moduleName) - 1,\
		moduleClass::Init,\
		moduleClass::Shutdown\
	};\
}\
RKIT_IMPLEMENT_PER_MODULE_FUNCTIONS

#endif	// RKIT_IS_DEBUG

#endif	// _WIN32
