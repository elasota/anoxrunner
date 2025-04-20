#pragma once

namespace rkit
{
	struct IJobQueue;
	struct IMallocDriver;

	template<class T>
	class UniquePtr;
}

namespace rkit::utils
{
	Result CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc);
}
