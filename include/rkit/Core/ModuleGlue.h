#pragma once


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
}

#endif
