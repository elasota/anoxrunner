#pragma once

#include <cstdint>

namespace rkit::data
{
	struct ContentID
	{
		static const size_t kSize = 32;

		uint8_t m_data[kSize];
	};
}
