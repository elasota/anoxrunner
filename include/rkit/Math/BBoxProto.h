#pragma once

#include <stddef.h>

namespace rkit::math
{
	template<class TComponent, size_t TSize>
	class BBox;

	using BBox3 = BBox<float, 3>;
}
