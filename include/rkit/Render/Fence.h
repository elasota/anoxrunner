#pragma once

#include <cstdint>
#include <limits>

namespace rkit::render
{
	typedef uint16_t TimelineID_t;
	typedef uint64_t TimelinePoint_t;

	enum class FenceType
	{
		kCPUWaitable,
		kGPUWaitable,
	};

	template<FenceType TFenceType>
	class FenceID
	{
	public:
		FenceID();
		explicit FenceID(TimelineID_t timelineID, TimelinePoint_t timelinePoint);
		FenceID(const FenceID &other) = default;

		bool IsValid() const;

	private:
		static const TimelineID_t kInvalidTimelineID = std::numeric_limits<TimelineID_t>::max();

		TimelinePoint_t m_timelinePoint;
		TimelineID_t m_timelineID;
	};

	typedef FenceID<FenceType::kCPUWaitable> CPUWaitableFence_t;
	typedef FenceID<FenceType::kGPUWaitable> GPUWaitableFence_t;
}

namespace rkit::render
{
	template<FenceType TFenceType>
	inline FenceID<TFenceType>::FenceID()
		: m_timelinePoint(0)
		, m_timelineID(kInvalidTimelineID)
	{
	}

	template<FenceType TFenceType>
	inline FenceID<TFenceType>::FenceID(TimelineID_t timelineID, TimelinePoint_t timelinePoint)
		: m_timelineID(timelineID)
		, m_timelinePoint(timelinePoint)
	{
	}
}
