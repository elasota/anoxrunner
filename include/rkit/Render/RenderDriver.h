#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	struct IRenderDriver : public ICustomDriver
	{
		virtual ~IRenderDriver() {}
	};
}
