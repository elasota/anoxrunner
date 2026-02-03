#pragma once

#include <cstdint>

#include "rkit/Core/JobQueue.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;

	struct IJobQueue;
}

namespace rkit { namespace utils
{
	struct IThreadPool
	{
		virtual ~IThreadPool() {}

		virtual PackedResultAndExtCode Close() = 0;
		virtual IJobQueue *GetJobQueue() const = 0;
		virtual const ISpan<JobType> &GetMainThreadJobTypes() const = 0;
		virtual const ISpan<JobType> &GetAllJobTypes() const = 0;
	};
} } // rkit::utils
