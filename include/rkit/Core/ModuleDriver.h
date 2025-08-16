#pragma once

#include <cstdint>

#include "rkit/Core/FourCC.h"

namespace rkit
{
	struct IModule;
	struct ModuleInitParameters;

	struct IModuleDriver
	{
		static const uint32_t kDefaultNamespace = RKIT_FOURCC('R', 'K', 'i', 't');

		virtual ~IModuleDriver() {}

		IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams);
		IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName);

	protected:
		virtual IModule *LoadModuleInternal(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams, IMallocDriver &mallocDriver) = 0;
	};
}

#include "rkit/Core/Drivers.h"

namespace rkit
{
	inline IModule *IModuleDriver::LoadModule(uint32_t moduleNamespace, const char *moduleName)
	{
		return this->LoadModule(moduleNamespace, moduleName, nullptr);
	}

	inline IModule *IModuleDriver::LoadModule(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams)
	{
		return this->LoadModuleInternal(moduleNamespace, moduleName, initParams, *GetDrivers().m_mallocDriver);
	}
}
