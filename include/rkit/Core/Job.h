#pragma once

#include "CoreDefs.h"
#include "RefCounted.h"

namespace rkit
{
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

	class JobSignaller : public RefCounted
	{
	public:
		virtual void SignalDone(const Result &result) = 0;
	};
}
