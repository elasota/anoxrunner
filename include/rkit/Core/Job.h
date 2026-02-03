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
		void SignalDone(const PackedResultAndExtCode &result);

	protected:
		virtual void SignalDoneImpl(const PackedResultAndExtCode &result) = 0;
	};
}

namespace rkit
{
	inline void JobSignaler::SignalDone(ResultCode resultCode)
	{
		this->SignalDoneImpl(PackedResultAndExtCode{ resultCode, 0 });
	}

	inline void JobSignaler::SignalDone(ResultCode resultCode, uint32_t extCode)
	{
		this->SignalDoneImpl(PackedResultAndExtCode{ resultCode, extCode });
	}

	inline void JobSignaler::SignalDone(const PackedResultAndExtCode &result)
	{
		this->SignalDoneImpl(result);
	}
}
