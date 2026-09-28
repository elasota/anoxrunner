#pragma once

#include "TimelinePoint.h"
#include "TimelineSignalType.h"

namespace rkit::render
{
	struct ITimelineFence;
	class TimelineSignalIntent
	{
	public:
		TimelineSignalIntent() = default;
		TimelineSignalIntent(ITimelineFence &fence, TimelineSignalType signalType, TimelinePoint_t timelinePoint);

		ITimelineFence *GetFence() const;
		TimelineSignalType GetType() const;
		TimelinePoint_t GetTimelinePoint() const;

	private:
		ITimelineFence *m_fence = nullptr;
		TimelineSignalType m_signalType = TimelineSignalType::kNone;
		TimelinePoint_t m_timelinePoint = 0;
	};
}

namespace rkit::render
{
	inline ITimelineFence *TimelineSignalIntent::GetFence() const
	{
		return m_fence;
	}

	inline TimelineSignalIntent::TimelineSignalIntent(ITimelineFence &fence, TimelineSignalType signalType, TimelinePoint_t timelinePoint)
		: m_fence(&fence)
		, m_signalType(signalType)
		, m_timelinePoint(timelinePoint)
	{
	}

	inline TimelineSignalType TimelineSignalIntent::GetType() const
	{
		return m_signalType;
	}

	inline TimelinePoint_t TimelineSignalIntent::GetTimelinePoint() const
	{
		return m_timelinePoint;
	}
}
