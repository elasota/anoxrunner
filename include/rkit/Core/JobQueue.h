#pragma once

namespace rkit
{
	class Job;

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

		virtual Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> *dependencies) = 0;

		virtual Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<RCPtr<Job> > &dependencies) = 0;

		// Waits for work from a job queue.
		// If "waitIfDepleted" is set, then wakeEvent must be an auto-reset event and terminatedEvent must not be signaled
		virtual RCPtr<Job> WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) = 0;

		virtual void WaitForJob(Job &job, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent, IEvent *specificThreadWakeEvent) = 0;

		virtual void Fault(const Result &result) = 0;

		virtual Result CheckFault() = 0;

		// Closes the job queue.  The shutdown procedure for the job queue should be:
		// - Close the job queue
		// - Wait for all participating threads to finish calling WaitForWork
		// - Destroy the job queue
		virtual Result Close() = 0;
	};
}
