#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Pair.h"

#include "TimelinePoint.h"

#include <cstdint>
#include <limits>

namespace rkit
{
	template<class T>
	class Span;

	template<class TFirst, class TSecond>
	class Pair;
}

namespace rkit::render
{
	struct IBinaryCPUWaitableFence;
	struct ICPUVisibleTimelineFence;

	struct ICPUFenceWaiter
	{
		virtual ~ICPUFenceWaiter() {};

		Result WaitForFence(ICPUVisibleTimelineFence &fence, TimelinePoint_t timelinePoint);
		Result WaitForFenceTimed(bool &outTimeout, ICPUVisibleTimelineFence &fence, TimelinePoint_t timelinePoint, uint64_t timeoutMSec);

		Result WaitForBinaryFence(IBinaryCPUWaitableFence &fence);
		Result WaitForBinaryFenceTimed(bool &outTimeout, IBinaryCPUWaitableFence &fence, uint64_t timeoutMSec);

		virtual Result WaitForFences(const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, bool waitAll) = 0;
		virtual Result WaitForFencesTimed(bool &outTimeout, const Span<const Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>> &timelineWaits, uint64_t timeoutMSec, bool waitAll) = 0;
		virtual Result WaitForBinaryFences(const Span<IBinaryCPUWaitableFence *const> &binaryWaits, bool waitAll) = 0;
		virtual Result WaitForBinaryFencesTimed(bool &outTimeout, const Span<IBinaryCPUWaitableFence *const> &binaryWaits, uint64_t timeoutMSec, bool waitAll) = 0;
	};

	struct ITimelineFence
	{
		virtual ~ITimelineFence() {}

		ICPUVisibleTimelineFence *ToCPUVisible();
		const ICPUVisibleTimelineFence *ToCPUVisible() const;

	protected:
		virtual ICPUVisibleTimelineFence *ToCPUVisibleInternal() = 0;
	};

	struct ICPUVisibleTimelineFence : public ITimelineFence
	{
		virtual Result SetValue(TimelinePoint_t value) = 0;
		virtual Result GetCurrentValue(TimelinePoint_t &outValue) const = 0;
	};

	struct IBinaryCPUWaitableFence
	{
		virtual ~IBinaryCPUWaitableFence() {}

		virtual Result ResetFence() = 0;
	};

	struct IBinaryGPUWaitableFence
	{
		virtual ~IBinaryGPUWaitableFence() {}
	};
} // rkit::render

#include "rkit/Core/Span.h"

namespace rkit::render
{
	inline ICPUVisibleTimelineFence *ITimelineFence::ToCPUVisible()
	{
		return this->ToCPUVisibleInternal();
	}

	inline const ICPUVisibleTimelineFence *ITimelineFence::ToCPUVisible() const
	{
		return const_cast<ITimelineFence *>(this)->ToCPUVisibleInternal();
	}

	inline Result ICPUFenceWaiter::WaitForFence(ICPUVisibleTimelineFence &fence, TimelinePoint_t timelinePoint)
	{
		rkit::Pair<ICPUVisibleTimelineFence *, TimelinePoint_t> fenceWait(&fence, timelinePoint);

		return this->WaitForFences(rkit::Span<const rkit::Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>>(&fenceWait, 1), true);
	}

	inline Result ICPUFenceWaiter::WaitForFenceTimed(bool &outTimeout, ICPUVisibleTimelineFence &fence, TimelinePoint_t timelinePoint, uint64_t timeoutMSec)
	{
		rkit::Pair<ICPUVisibleTimelineFence *, TimelinePoint_t> fenceWait(&fence, timelinePoint);
		return this->WaitForFencesTimed(outTimeout, rkit::Span<const rkit::Pair<ICPUVisibleTimelineFence *, TimelinePoint_t>>(&fenceWait, 1), timeoutMSec, true);
	}

	inline Result ICPUFenceWaiter::WaitForBinaryFence(IBinaryCPUWaitableFence &fence)
	{
		IBinaryCPUWaitableFence *fencePtr = &fence;
		return this->WaitForBinaryFences(rkit::Span<IBinaryCPUWaitableFence * const>(&fencePtr, 1), true);
	}

	inline Result ICPUFenceWaiter::WaitForBinaryFenceTimed(bool &outTimeout, IBinaryCPUWaitableFence &fence, uint64_t timeoutMSec)
	{
		IBinaryCPUWaitableFence *fencePtr = &fence;
		return this->WaitForBinaryFencesTimed(outTimeout, rkit::Span<IBinaryCPUWaitableFence *const>(&fencePtr, 1), timeoutMSec, true);
	}
}
