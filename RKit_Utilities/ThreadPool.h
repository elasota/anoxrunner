#pragma once

#include "rkit/Utilities/ThreadPool.h"

namespace rkit
{
	struct IUtilitiesDriver;
}

namespace rkit { namespace utils
{
	class ThreadPoolBase : public IThreadPool
	{
	public:
		static Result Create(UniquePtr<ThreadPoolBase> &outThreadPool, const IUtilitiesDriver &utils, uint32_t numThreads);
	};
} } // rkit::utils
