#pragma once

namespace rkit
{
	class Job;
	class JobSignaler;

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

	// This should not be constructed, since it can create additional temporaries
	class JobDependencyList;

	struct IJobQueue
	{
		virtual ~IJobQueue() {}

		virtual Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const JobDependencyList &dependencies) = 0;

		virtual Result CreateSignaledJob(RCPtr<JobSignaler> &outSignaler, RCPtr<Job> &outJob) = 0;

		virtual Result CreateSignalJobRunner(UniquePtr<IJobRunner> &outJobRunner, const RCPtr<JobSignaler> &signaller) = 0;

		// Waits for work from a job queue.
		// If "waitIfDepleted" is set, then wakeEvent must be an auto-reset event and terminatedEvent must not be signaled
		virtual RCPtr<Job> WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) = 0;

		virtual void WaitForJob(Job &job, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent) = 0;

		virtual void Fault(const Result &result) = 0;

		virtual Result CheckFault() = 0;

		// Closes the job queue.  The shutdown procedure for the job queue should be:
		// - Close the job queue
		// - Wait for all participating threads to finish calling WaitForWork
		// - Ensure any outstanding job signallers are destroyed
		// - Destroy the job queue
		virtual Result Close() = 0;
	};
}

// Include JobDependencyList since it's nearly always needed for this
#include "rkit/Core/JobDependencyList.h"
