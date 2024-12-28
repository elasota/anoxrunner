#pragma once

#include <cstdint>

#include "rkit/Core/JobQueue.h"

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;

	struct IJobQueue;
}

namespace rkit::utils
{
	struct IThreadPool
	{
		virtual ~IThreadPool() {}

		virtual Result Close() = 0;
		virtual IJobQueue *GetJobQueue() const = 0;
		virtual JobType GetMainThreadJobType() const = 0;
	};
}
