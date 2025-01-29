#pragma once

#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Span.h"

namespace rkit
{
	namespace priv
	{
		template<JobType ...TJobTypes>
		struct JobTypeListHelper
		{
		};

		template<JobType TJobType>
		struct JobTypeListHelper<TJobType>
		{
			static const size_t kCount = 1;

			static JobType Resolve(size_t index);
		};

		template<JobType TJobType, JobType ...TMoreJobTypes>
		struct JobTypeListHelper<TJobType, TMoreJobTypes...>
		{
			static const size_t kCount = JobTypeListHelper<TMoreJobTypes...>::kCount + 1;

			static JobType Resolve(size_t index);
		};
	}

	template<JobType ...TJobTypes>
	class JobTypeList final : public ISpan<JobType>
	{
	public:
		size_t Count() const override;
		JobType operator[](size_t index) const override;
	};
}

#include "RKitAssert.h"

namespace rkit::priv
{
	template<JobType TJobType>
	JobType JobTypeListHelper<TJobType>::Resolve(size_t index)
	{
		RKIT_ASSERT(index == 0);
		return TJobType;
	}

	template<JobType TJobType, JobType ...TMoreJobTypes>
	JobType JobTypeListHelper<TJobType, TMoreJobTypes...>::Resolve(size_t index)
	{
		if (index == 0)
			return JobType;

		return JobTypeListHelper<TMoreJobTypes...>::Resolve(index - 1);
	}
}

namespace rkit
{
	template<JobType ...TJobTypes>
	size_t JobTypeList<TJobTypes...>::Count() const
	{
		return priv::JobTypeListHelper<TJobTypes...>::kCount;
	}

	template<JobType ...TJobTypes>
	JobType JobTypeList<TJobTypes...>::operator[](size_t index) const
	{
		return priv::JobTypeListHelper<TJobTypes...>::Resolve(index);
	}
}

