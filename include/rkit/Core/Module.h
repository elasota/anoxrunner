#pragma once

namespace rkit
{
	struct Drivers;

	struct ModuleInitParameters
	{
	};

	struct IModule
	{
		virtual ~IModule() {};

		virtual Result Init(const ModuleInitParameters *initParams) = 0;
		virtual void Unload() = 0;
	};
}
