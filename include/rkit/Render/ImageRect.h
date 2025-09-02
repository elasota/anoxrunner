#pragma once

#include <cstdint>

namespace rkit { namespace render {
	struct ImageRect2D
	{
		int32_t m_x = 0;
		int32_t m_y = 0;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
	};

	struct ImageRect3D
	{
		int32_t m_x = 0;
		int32_t m_y = 0;
		int32_t m_z = 0;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_depth = 0;
	};
} } // rkit::render
