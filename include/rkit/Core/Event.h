#pragma once

#include <cstdint>

namespace rkit
{
	struct IEvent
	{
		virtual ~IEvent() {}

		virtual void Signal() = 0;
		virtual void Wait() = 0;
		virtual bool TimedWait(uint32_t msec) = 0;
	};
}
