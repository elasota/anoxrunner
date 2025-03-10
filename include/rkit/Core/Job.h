#pragma once

#include "RefCounted.h"

namespace rkit
{
	struct Result;

	template<class T>
	class RCPtr;

	struct IJobContext
	{
	};

	struct IJobRunner
	{
		virtual ~IJobRunner() {}

		virtual Result Run() = 0;
	};

	class Job : public RefCounted
	{
	public:
		virtual void Run() = 0;
	};
}
