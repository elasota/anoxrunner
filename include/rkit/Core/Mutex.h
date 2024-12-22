#pragma once

#include <cstdint>

namespace rkit
{
	struct IMutex
	{
		virtual ~IMutex() {}

		virtual void Lock() = 0;
		virtual bool TryLock(uint32_t maxMSec) = 0;
		virtual void Unlock() = 0;
	};
}
