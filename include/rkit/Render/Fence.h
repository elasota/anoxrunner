#pragma once

#include <cstdint>
#include <limits>

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	typedef uint64_t TimelinePoint_t;

	struct IBinaryCPUWaitableFence
	{
		virtual ~IBinaryCPUWaitableFence() {}

		virtual Result WaitFor() = 0;
		virtual Result WaitForTimed(uint64_t timeoutMSec) = 0;
		virtual Result ResetFence() = 0;
	};

	struct IBinaryGPUWaitableFence
	{
		virtual ~IBinaryGPUWaitableFence() {}
	};
}
