#pragma once

namespace rkit
{
	class Job;
	class JobSignaller;

	template<class T>
	class RCPtr;

	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;

	struct IEvent;
	struct IJobRunner;

	enum class JobType
	{
		kNormalPriority,
		kLowPriority,
		kIO,

		kUser0,
		kUser1,
		kUser2,
		kUser3,
		kUser4,
		kUser5,
		kUser6,
		kUser7,
		kUser8,
		kUser9,
		kUser10,
		kUser11,
		kUser12,

		kCount,
	};

	struct IJobQueue
	{
		virtual ~IJobQueue() {}

		virtual Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> &dependencies) = 0;
		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const Span<Job *> &dependencies);
		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, Job *dependency);

		virtual Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<RCPtr<Job> > &dependencies) = 0;
		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const Span<const RCPtr<Job> > &dependencies);
		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const RCPtr<Job> &dependency);

		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, std::nullptr_t dependencies);

		virtual Result CreateSignalledJob(RCPtr<JobSignaller> &outSignaler, RCPtr<Job> &outJob) = 0;

		virtual Result CreateSignalJobRunner(UniquePtr<IJobRunner> &outJobRunner, const RCPtr<JobSignaller> &signaller) = 0;

		// Waits for work from a job queue.
		// If "waitIfDepleted" is set, then wakeEvent must be an auto-reset event and terminatedEvent must not be signaled
		virtual RCPtr<Job> WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) = 0;

		virtual void WaitForJob(Job &job, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent) = 0;

		virtual void Fault(const Result &result) = 0;

		virtual Result CheckFault() = 0;

		// Closes the job queue.  The shutdown procedure for the job queue should be:
		// - Close the job queue
		// - Wait for all participating threads to finish calling WaitForWork
		// - Destroy the job queue
		virtual Result Close() = 0;
	};
}

#include "RefCounted.h"

namespace rkit
{
	inline Result IJobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const Span<Job *> &dependencies)
	{
		return this->CreateJob(outJob, jobType, std::move(jobRunner), dependencies.ToValueISpan());
	}

	inline Result IJobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, Job *dependency)
	{
		if (dependency != nullptr)
			return this->CreateJob(outJob, jobType, std::move(jobRunner), Span<Job *>(&dependency, 1));
		else
			return this->CreateJob(outJob, jobType, std::move(jobRunner), nullptr);
	}

	inline Result IJobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const Span<const RCPtr<Job> > &dependencies)
	{
		return this->CreateJob(outJob, jobType, std::move(jobRunner), dependencies.ToValueISpan());
	}

	inline Result IJobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const RCPtr<Job> &dependency)
	{
		if (dependency.IsValid())
			return this->CreateJob(outJob, jobType, std::move(jobRunner), Span<const RCPtr<Job>>(&dependency, 1));
		else
			return this->CreateJob(outJob, jobType, std::move(jobRunner), nullptr);
	}

	inline Result IJobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, std::nullptr_t dependencies)
	{
		return this->CreateJob(outJob, jobType, std::move(jobRunner), Span<Job *>());
	}
}
