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

		virtual IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams) = 0;

		inline IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName)
		{
			return this->LoadModule(moduleNamespace, moduleName, nullptr);
		}
	};
}
