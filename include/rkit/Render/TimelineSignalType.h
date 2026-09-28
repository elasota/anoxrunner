#pragma once

namespace rkit::render
{
	enum class TimelineSignalType
	{
		kNone,
		kGPUWaitable,
		kCPUWaitable,
		kCPUGPUWaitable,
	};
}
