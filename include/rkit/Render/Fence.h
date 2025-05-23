#pragma once

#include "rkit/Core/CoreDefs.h"

#include <cstdint>
#include <limits>

namespace rkit { namespace render
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
} } // rkit::render
