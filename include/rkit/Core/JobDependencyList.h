#pragma once

#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Span.h"

namespace rkit
{
	class JobDependencyList
	{
	public:
		JobDependencyList();
		JobDependencyList(std::nullptr_t);
		JobDependencyList(Job *ptr);
		JobDependencyList(const RCPtr<Job> &rcPtr);
		JobDependencyList(const Span<Job *> &span);
		JobDependencyList(const Span<RCPtr<Job>> &span);
		JobDependencyList(const ConstSpan<Job *> &span);
		JobDependencyList(const ConstSpan<RCPtr<Job>> &span);
		JobDependencyList(const ISpan<Job *> &ispan);
		JobDependencyList(const ISpan<RCPtr<Job>> &ispan);

		JobDependencyList(const JobDependencyList &) = delete;

		const ISpan<Job *> &GetSpan() const;

	private:
		CallbackSpan<Job *, const void *> m_callbackSpan;

		static Job *GetJobFromRawPtr(const void *const &ptr, size_t index);
		static Job *GetJobFromJobPtrList(const void *const &ptr, size_t index);
		static Job *GetJobFromJobRCPtrList(const void *const &ptr, size_t index);
		static Job *GetJobFromJobPtrISpan(const void *const &ptr, size_t index);
		static Job *GetJobFromJobRCPtrISpan(const void *const &ptr, size_t index);
	};

	inline JobDependencyList::JobDependencyList()
		: m_callbackSpan()
	{
	}

	inline JobDependencyList::JobDependencyList(std::nullptr_t)
		: m_callbackSpan()
	{
	}

	inline JobDependencyList::JobDependencyList(Job *ptr)
		: m_callbackSpan((ptr != nullptr) ? GetJobFromRawPtr : nullptr, ptr, (ptr != nullptr) ? 1 : 0)
	{
	}

	inline JobDependencyList::JobDependencyList(const RCPtr<Job> &rcPtr)
		: m_callbackSpan(rcPtr.IsValid() ? GetJobFromRawPtr : nullptr, rcPtr.Get(), rcPtr.IsValid() ? 1 : 0)
	{
	}

	inline JobDependencyList::JobDependencyList(const Span<Job *> &span)
		: m_callbackSpan(GetJobFromJobPtrList, span.Ptr(), span.Count())
	{
	}

	inline JobDependencyList::JobDependencyList(const ConstSpan<Job *> &span)
		: m_callbackSpan(GetJobFromJobPtrList, span.Ptr(), span.Count())
	{
	}

	inline JobDependencyList::JobDependencyList(const Span<RCPtr<Job>> &span)
		: m_callbackSpan(GetJobFromJobRCPtrList, span.Ptr(), span.Count())
	{
	}

	inline JobDependencyList::JobDependencyList(const ConstSpan<RCPtr<Job>> &span)
		: m_callbackSpan(GetJobFromJobRCPtrList, span.Ptr(), span.Count())
	{
	}

	inline JobDependencyList::JobDependencyList(const ISpan<Job *> &ispan)
		: m_callbackSpan(GetJobFromJobPtrISpan, &ispan, ispan.Count())
	{
	}

	inline JobDependencyList::JobDependencyList(const ISpan<RCPtr<Job>> &ispan)
		: m_callbackSpan(GetJobFromJobRCPtrISpan, &ispan, ispan.Count())
	{
	}

	inline const ISpan<Job *> &JobDependencyList::GetSpan() const
	{
		return m_callbackSpan;
	}

	inline Job *JobDependencyList::GetJobFromRawPtr(const void *const &ptr, size_t index)
	{
		RKIT_ASSERT(index == 0);
		return const_cast<Job *>(static_cast<const Job *>(ptr));
	}

	inline Job *JobDependencyList::GetJobFromJobPtrList(const void *const &ptr, size_t index)
	{
		return static_cast<Job * const*>(ptr)[index];
	}

	inline Job *JobDependencyList::GetJobFromJobRCPtrList(const void *const &ptr, size_t index)
	{
		return static_cast<const RCPtr<Job> *>(ptr)[index].Get();
	}

	inline Job *JobDependencyList::GetJobFromJobPtrISpan(const void *const &ptr, size_t index)
	{
		const ISpan<Job *> &span = *static_cast<const ISpan<Job *> *>(ptr);
		return span[index];
	}

	inline Job *JobDependencyList::GetJobFromJobRCPtrISpan(const void *const &ptr, size_t index)
	{
		const ISpan<RCPtr<Job>> &span = *static_cast<const ISpan<RCPtr<Job>> *>(ptr);
		return span[index].Get();
	}
}
