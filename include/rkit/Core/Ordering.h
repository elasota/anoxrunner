#pragma once

#include <stdint.h>

namespace rkit
{
	enum class Ordering : int8_t
	{
		kEqual = 0,
		kLess = -1,
		kGreater = 1,
		kUnordered = -2,
	};
}
