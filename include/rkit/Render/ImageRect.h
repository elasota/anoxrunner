#pragma once

#include <cstdint>

namespace rkit::render
{
	struct ImageRect2D
	{
		int32_t m_x = 0;
		int32_t m_y = 0;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
	};
}
