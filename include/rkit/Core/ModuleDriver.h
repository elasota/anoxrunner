#pragma once

#include <cstdint>

namespace rkit
{
	struct IModule;

	struct IModuleDriver
	{
		static const uint32_t kDefaultNamespace = 0x74694b52;

		virtual ~IModuleDriver() {}

		virtual IModule *LoadModule(uint32_t moduleNamespace, const char *moduleName) = 0;
	};
}
