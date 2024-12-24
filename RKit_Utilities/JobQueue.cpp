#include "rkit/Core/Drivers.h"
#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "JobQueue.h"

namespace rkit::utils
{
	class JobQueue;

	class JobImpl final : public Job
	{
	public:
		friend class JobQueue;

		explicit JobImpl(JobQueue &jobQueue, UniquePtr<IJobRunner> &&jobRunner, size_t numDependencies);
		~JobImpl();

		Result ReserveDownstreamDependency();
		RCPtr<JobImpl> *GetLastDownstreamDependency();

	private:
		static const size_t kStaticDownstreamListSize = 8;

		size_t m_numWaitingDependencies = 0;

		JobQueue &m_jobQueue;
		UniquePtr<IJobRunner> m_jobRunner;

		StaticArray<RCPtr<JobImpl>, kStaticDownstreamListSize> m_staticDownstreamList;
		size_t m_numStaticDownstream = 0;

		Vector<RCPtr<JobImpl>> m_dynamicDownstream;

		RCPtr<JobImpl> m_nextRunnableJob;
		JobImpl *m_prevRunnableJob = nullptr;

		bool m_isCompleted;
	};

	class JobQueue final : public IJobQueue
	{
	public:
		friend class JobImpl;

		explicit JobQueue(IMallocDriver *alloc);
		~JobQueue();

		Result CreateJob(RCPtr<Job> &outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> &dependencies) override;

		// Waits for work from a job queue.
		// If "waitIfDepleted" is set, then wakeEvent must be an auto-reset event and terminatedEvent must not be signalled
		RCPtr<Job> WaitForWork(JobType jobType, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) override;

		void Fault(const Result &result) override;

		Result CheckFault() override;
		Result Close() override;

		Result Init();

	private:
		static const size_t kNumJobCategories = static_cast<size_t>(rkit::JobType::kCount);
		static const size_t kNumWaitableThreadCategories = kNumJobCategories + 1;

		void AddRunnableJob(const RCPtr<JobImpl> &job, size_t jobTypeIndex);

		struct ParticipatingThreadInfo
		{
			ParticipatingThreadInfo *m_next = nullptr;
			ParticipatingThreadInfo *m_prev = nullptr;

			bool m_isTerminating = false;

			IEvent* m_wakeEvent = nullptr;
			IEvent* m_terminatedEvent = nullptr;
		};

		struct ParticipatingThreadList
		{
			ParticipatingThreadInfo *m_first = nullptr;
			ParticipatingThreadInfo *m_last = nullptr;

			bool m_isClosing = false;

			UniquePtr<IMutex> m_mutex;
		};

		struct RunnableJobList
		{
			RCPtr<JobImpl> m_first;
			JobImpl *m_last = nullptr;

			bool m_isClosing = false;

			UniquePtr<IMutex> m_mutex;
		};

		void PrivClose();

		UniquePtr<IMutex> m_depGraphMutex;
		UniquePtr<IMutex> m_resultMutex;

		IMallocDriver *m_alloc;
		Result m_result;

		StaticArray<ParticipatingThreadList, kNumWaitableThreadCategories> m_waitingThreads;
		StaticArray<RunnableJobList, kNumJobCategories> m_runnableJobs;

		bool m_isInitialized;
	};

	JobImpl::JobImpl(JobQueue &jobQueue, UniquePtr<IJobRunner> &&jobRunner, size_t numDependencies)
		: m_jobQueue(jobQueue)
		, m_jobRunner(std::move(jobRunner))
		, m_numWaitingDependencies(numDependencies)
		, m_numStaticDownstream(0)
	{
	}

	JobImpl::~JobImpl()
	{
	}

	Result JobImpl::ReserveDownstreamDependency()
	{
		if (m_numStaticDownstream < kStaticDownstreamListSize)
		{
			m_numStaticDownstream++;
			return ResultCode::kOK;
		}
		else
			return m_dynamicDownstream.Append(RCPtr<JobImpl>());
	}

	RCPtr<JobImpl> *JobImpl::GetLastDownstreamDependency()
	{
		if (m_dynamicDownstream.Count() > 0)
			return &m_dynamicDownstream[m_dynamicDownstream.Count() - 1];

		RKIT_ASSERT(m_numStaticDownstream > 0);

		return &m_staticDownstreamList[m_numStaticDownstream - 1];
	}

	JobQueue::JobQueue(IMallocDriver *alloc)
		: m_alloc(alloc)
		, m_isInitialized(false)
	{
	}

	JobQueue::~JobQueue()
	{
		if (m_isInitialized)
		{
			for (const ParticipatingThreadList &threadList : m_waitingThreads)
			{
				RKIT_ASSERT(threadList.m_first == nullptr);
			}
		}
	}

	void JobQueue::PrivClose()
	{
		if (m_isInitialized)
		{
			for (RunnableJobList &rjl : m_runnableJobs)
			{
				MutexLock lock(*rjl.m_mutex);

				rjl.m_isClosing = true;

				while (rjl.m_first.IsValid())
				{
					rjl.m_first = rjl.m_first->m_nextRunnableJob;
				}

				rjl.m_last = nullptr;
			}

			for (ParticipatingThreadList &threadList : m_waitingThreads)
			{
				for (;;)
				{
					ParticipatingThreadInfo *pti = nullptr;

					{
						MutexLock lock(*threadList.m_mutex);

						pti = threadList.m_first;

						if (!pti)
							break;

						threadList.m_first = pti->m_next;
						if (!threadList.m_first)
							threadList.m_last = nullptr;

						pti->m_prev = nullptr;
						pti->m_next = nullptr;

						pti->m_isTerminating = true;
					}

					pti->m_wakeEvent->Signal();
					pti->m_terminatedEvent->Wait();
				}
			}
		}
	}

	Result JobQueue::CreateJob(RCPtr<Job> &outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> &dependencies)
	{
		UniquePtr<IJobRunner> jobRunnerTemp(std::move(jobRunner));

		RCPtr<JobImpl> resultJob;
		RKIT_CHECK(NewWithAlloc<JobImpl>(resultJob, m_alloc, *this, std::move(jobRunnerTemp), dependencies.Count()));

		RunnableJobList &ptl = m_runnableJobs[static_cast<size_t>(jobType)];

		{
			MutexLock lock(*m_depGraphMutex);

			if (ptl.m_isClosing)
				return ResultCode::kOK;

			for (Job *job : dependencies)
			{
				RKIT_CHECK(static_cast<JobImpl *>(job)->ReserveDownstreamDependency());
			}

			for (Job *job : dependencies)
			{
				*(static_cast<JobImpl *>(job)->GetLastDownstreamDependency()) = resultJob;
			}
		}

		if (dependencies.Count() == 0)
			AddRunnableJob(resultJob, static_cast<size_t>(jobType));

		outJob = std::move(resultJob);

		return ResultCode::kOK;
	}


	void JobQueue::AddRunnableJob(const RCPtr<JobImpl> &job, size_t jobTypeIndex)
	{
		RunnableJobList &rjl = m_runnableJobs[jobTypeIndex];

		{
			MutexLock lock(*rjl.m_mutex);

			if (rjl.m_isClosing)
				return;

			job->m_prevRunnableJob = rjl.m_last;
			job->m_nextRunnableJob = nullptr;

			if (rjl.m_last)
				rjl.m_last->m_nextRunnableJob = job;
			else
				rjl.m_first = job;

			rjl.m_last = job;
		}

		ParticipatingThreadList &allPtl = m_waitingThreads[static_cast<size_t>(JobType::kAnyJob)];
		ParticipatingThreadList &jobTypePtl = m_waitingThreads[jobTypeIndex];

		MutexLock allLock(*allPtl.m_mutex);
		MutexLock jobTypeLock(*jobTypePtl.m_mutex);

		ParticipatingThreadList *ptlToKick = nullptr;

		if (jobTypePtl.m_first)
			ptlToKick = &jobTypePtl;
		else if (allPtl.m_first)
			ptlToKick = &allPtl;

		if (ptlToKick && !ptlToKick->m_isClosing)
		{
			ParticipatingThreadInfo *pti = ptlToKick->m_first;

			ptlToKick->m_first = pti->m_next;
			if (!ptlToKick->m_first)
				ptlToKick->m_last = nullptr;

			pti->m_prev = nullptr;
			pti->m_next = nullptr;

			if (!pti->m_isTerminating)
				pti->m_wakeEvent->Signal();
		}
	}

	RCPtr<Job> JobQueue::WaitForWork(JobType jobType, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent)
	{
		ParticipatingThreadList &ptl = m_waitingThreads[static_cast<size_t>(jobType)];

		ParticipatingThreadInfo pti;
		pti.m_wakeEvent = wakeEvent;
		pti.m_terminatedEvent = terminatedEvent;

		Span<RunnableJobList> runnableJobs = (jobType == JobType::kAnyJob) ? (m_runnableJobs.ToSpan()) : Span<RunnableJobList>(&m_runnableJobs[static_cast<size_t>(jobType)], 1);

		for (;;)
		{
			RCPtr<JobImpl> job;

			for (RunnableJobList &jobList : runnableJobs)
			{
				MutexLock lock(*jobList.m_mutex);

				job = jobList.m_first;

				if (job)
				{
					JobImpl *nextJob = job->m_nextRunnableJob;
					jobList.m_first = nextJob;
					if (!jobList.m_first)
						jobList.m_last = nullptr;

					if (nextJob)
						nextJob->m_prevRunnableJob = nullptr;

					break;
				}
			}

			if (job)
			{
				job->m_nextRunnableJob = nullptr;
				job->m_prevRunnableJob = nullptr;

				return job;
			}

			if (!waitIfDepleted)
				break;

			{
				MutexLock lock(*ptl.m_mutex);
				if (ptl.m_isClosing)
					break;

				pti.m_prev = ptl.m_last;

				if (ptl.m_last)
					ptl.m_last->m_next = &pti;

				ptl.m_last = &pti;
			}

			pti.m_wakeEvent->Wait();

			if (pti.m_isTerminating)
				break;
		}

		if (terminatedEvent)
			terminatedEvent->Signal();

		return RCPtr<Job>();
	}

	void JobQueue::Fault(const Result &result)
	{
		MutexLock lock(*m_resultMutex);
		if (!result.IsOK())
			m_result = result;
	}

	Result JobQueue::CheckFault()
	{
		Result result;

		{
			MutexLock lock(*m_resultMutex);
			result = m_result;
		}

		return result;
	}

	Result JobQueue::Close()
	{
		PrivClose();
		return CheckFault();
	}

	Result JobQueue::Init()
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;

		RKIT_CHECK(sysDriver->CreateMutex(m_depGraphMutex));
		RKIT_CHECK(sysDriver->CreateMutex(m_resultMutex));

		for (ParticipatingThreadList &ptl : m_waitingThreads)
		{
			RKIT_CHECK(sysDriver->CreateMutex(ptl.m_mutex));
		}

		for (RunnableJobList &rjl : m_runnableJobs)
		{
			RKIT_CHECK(sysDriver->CreateMutex(rjl.m_mutex));
		}

		return ResultCode::kOK;
	}
}

rkit::Result rkit::utils::CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc)
{
	UniquePtr<JobQueue> jobQueue;
	RKIT_CHECK(NewWithAlloc<JobQueue>(jobQueue, alloc, alloc));
	RKIT_CHECK(jobQueue->Init());

	outJobQueue = std::move(jobQueue);

	return ResultCode::kOK;
}
