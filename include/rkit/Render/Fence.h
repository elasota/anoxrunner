#pragma once

#include <cstdint>
#include <limits>

namespace rkit::render
{
	typedef uint64_t TimelinePoint_t;

	struct IBinaryCPUWaitableFence
	{
		virtual ~IBinaryCPUWaitableFence() {}
	};

	struct IBinaryGPUWaitableFence
	{
		virtual ~IBinaryGPUWaitableFence() {}
	};
}
