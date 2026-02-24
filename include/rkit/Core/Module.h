#pragma once

#include "rkit/Core/Result.h"

namespace rkit
{
	struct Drivers;

	struct ModuleInitParameters
	{
	};

	struct IModule
	{
	public:
		virtual Result Init(const ModuleInitParameters *initParams) = 0;
		virtual void Unload() = 0;

	protected:
		virtual ~IModule() {};
	};
}
