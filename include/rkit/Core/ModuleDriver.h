#pragma once

#include <cstdint>

namespace rkit
{
	struct IModule;
	struct ModuleInitParameters;

	struct IModuleDriver
	{
		static const uint32_t kDefaultNamespace = 0x74694b52;

		virtual ~IModuleDriver() {}

		virtual IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName, const ModuleInitParameters *initParams) = 0;

		inline IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName)
		{
			return this->LoadModule(moduleNamespace, moduleName, nullptr);
		}
	};
}
