#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Pair.h"

#include <cstdint>
#include <limits>

namespace rkit
{
	template<class T>
	class Span;

	template<class TFirst, class TSecond>
	class Pair;
}

namespace rkit { namespace render
{
	struct IBinaryCPUWaitableFence;
	struct ICPUVisibleTimelineFence;

	typedef uint64_t TimelinePoint_t;

	struct ICPUFenceWaiter
	{
		virtual ~ICPUFenceWaiter() {};

		Result WaitForFence(ICPUVisibleTimelineFence &fence, TimelinePoint_t timelinePoint);
		Result WaitForFenceTimed(ICPUVisibleTimelineFence &fence, TimelinePoint_t timelinePoint, uint64_t timeoutMSec);

		Result WaitForBinaryFence(IBinaryCPUWaitableFence &fence);
		Result WaitForBinaryFenceTimed(IBinaryCPUWaitableFence &fence, uint64_t timeoutMSec);

		virtual Result WaitForFences(const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, bool waitAll) = 0;
		virtual Result WaitForFencesTimed(bool &outTimeout, const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, uint64_t timeoutMSec, bool waitAll) = 0;
		virtual Result WaitForBinaryFences(const Span<IBinaryCPUWaitableFence *> &binaryWaits, bool waitAll) = 0;
		virtual Result WaitForBinaryFencesTimed(bool &outTimeout, const Span<IBinaryCPUWaitableFence *> &binaryWaits, uint64_t timeoutMSec, bool waitAll) = 0;
	};

	struct ITimelineFence
	{
		virtual ~ITimelineFence() {}
	};

	struct ICPUVisibleTimelineFence : public ITimelineFence
	{
		virtual Result SetValue(TimelinePoint_t value) = 0;
		virtual Result GetCurrentValue(TimelinePoint_t &outValue) const = 0;
	};

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
