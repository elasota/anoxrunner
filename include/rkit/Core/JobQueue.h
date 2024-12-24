#pragma once

namespace rkit
{
	class Job;
	struct Result;

	template<class T>
	class RCPtr;

	template<class T>
	class UniquePtr;

	template<class T>
	struct ISpan;

	struct IJobRunner;

	enum class JobType
	{
		kNormalPriority,
		kLowPriority,
		kIO,

		kCount,

		kAnyJob,
	};

	struct IJobQueue
	{
		virtual ~IJobQueue() {}

		virtual Result CreateJob(RCPtr<Job> &outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job*> &dependencies) = 0;

		virtual RCPtr<Job> WaitForWork(JobType jobType, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) = 0;
		virtual void Fault(const Result &result) = 0;

		virtual Result CheckFault() = 0;

		// Closes the job queue.  The shutdown procedure for the job queue should be:
		// - Close the job queue
		// - Wait for all participating threads to finish calling WaitForWork
		// - Destroy the job queue
		virtual Result Close() = 0;
	};
}
