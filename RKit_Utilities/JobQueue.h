#pragma once

namespace rkit
{
	struct Result;
	struct IJobQueue;
	struct IMallocDriver;

	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	Result CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc);
}
