#pragma once

#include <cstddef>

namespace rkit
{
	struct IShellDriver
	{
		virtual size_t GetArgC() const = 0;
		virtual const char *GetArgV(unsigned int index) const = 0;
	};
}
