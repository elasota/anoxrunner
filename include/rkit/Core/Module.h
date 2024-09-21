#pragma once

namespace rkit
{
	struct Drivers;
	struct Result;

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
