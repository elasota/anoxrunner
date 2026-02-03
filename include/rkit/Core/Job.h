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

	class JobSignaler : public RefCounted
	{
	public:
		void SignalDone(ResultCode resultCode);
		void SignalDone(ResultCode resultCode, uint32_t extCode);
		void SignalDone(PackedResultAndExtCode result);

	protected:
		virtual void SignalDoneImpl(PackedResultAndExtCode result) = 0;
	};
}

namespace rkit
{
	inline void JobSignaler::SignalDone(ResultCode resultCode)
	{
		this->SignalDoneImpl(utils::PackResult(resultCode));
	}

	inline void JobSignaler::SignalDone(ResultCode resultCode, uint32_t extCode)
	{
		this->SignalDoneImpl(utils::PackResult(resultCode, extCode));
	}

	inline void JobSignaler::SignalDone(PackedResultAndExtCode result)
	{
		this->SignalDoneImpl(result);
	}
}
