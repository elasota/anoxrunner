#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct IBuildDriver : public rkit::ICustomDriver
	{
	};
}
