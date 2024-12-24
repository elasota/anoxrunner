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

		explicit JobImpl(JobQueue &jobQueue, UniquePtr<IJobRunner> &&jobRunner, size_t numDependencies, JobType jobType);
		~JobImpl();

		Result ReserveDownstreamDependency();
		RCPtr<JobImpl> *GetLastDownstreamDependency();

		void Run() override;

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

		JobType m_jobType;

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
		static const size_t kNumJobCategories = static_cast<size_t>(rkit::JobType::kCount) + 1;

		void AddRunnableJob(const RCPtr<JobImpl> &job, size_t jobTypeIndex);
		void JobDone(JobImpl *job);
		void DependencyDone(const RCPtr<JobImpl> &job);

		struct ParticipatingThreadInfo
		{
			ParticipatingThreadInfo *m_next = nullptr;
			ParticipatingThreadInfo *m_prev = nullptr;

			RCPtr<JobImpl> m_job;

			bool m_isTerminating = false;

			IEvent* m_wakeEvent = nullptr;
			IEvent* m_terminatedEvent;
		};

		struct CategoryInfo
		{
			ParticipatingThreadInfo *m_firstWaitingThread = nullptr;
			ParticipatingThreadInfo *m_lastWaitingThread = nullptr;

			RCPtr<JobImpl> m_firstJob;
			JobImpl *m_lastJob = nullptr;

			bool m_isClosing = false;
		};

		void PrivClose();

		UniquePtr<IMutex> m_distributorMutex;
		UniquePtr<IMutex> m_depGraphMutex;
		UniquePtr<IMutex> m_resultMutex;

		IMallocDriver *m_alloc;
		Result m_result;

		StaticArray<CategoryInfo, kNumJobCategories> m_categories;

		bool m_isInitialized;
	};

	JobImpl::JobImpl(JobQueue &jobQueue, UniquePtr<IJobRunner> &&jobRunner, size_t numDependencies, JobType jobType)
		: m_jobQueue(jobQueue)
		, m_jobRunner(std::move(jobRunner))
		, m_numWaitingDependencies(numDependencies)
		, m_numStaticDownstream(0)
		, m_jobType(jobType)
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

	void JobImpl::Run()
	{
		Result result = m_jobRunner->Run();

		if (!result.IsOK())
			m_jobQueue.Fault(result);

		m_jobRunner.Reset();
		m_jobQueue.JobDone(this);
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
			for (const CategoryInfo &catInfo : m_categories)
			{
				RKIT_ASSERT(catInfo.m_firstWaitingThread == nullptr);
				RKIT_ASSERT(catInfo.m_firstJob == nullptr);
			}
		}
	}

	void JobQueue::PrivClose()
	{
		if (m_isInitialized)
		{
			{
				MutexLock lock(*m_distributorMutex);

				for (CategoryInfo &ci : m_categories)
				{
					ci.m_isClosing = true;

					ci.m_firstJob = nullptr;
					ci.m_lastJob = nullptr;
				}
			}

			for (CategoryInfo &ci : m_categories)
			{
				for (;;)
				{
					ParticipatingThreadInfo *pti = nullptr;

					{
						MutexLock lock(*m_distributorMutex);

						pti = ci.m_firstWaitingThread;

						if (!pti)
							break;

						ci.m_firstWaitingThread = pti->m_next;
						if (!ci.m_firstWaitingThread)
							ci.m_lastWaitingThread = nullptr;

						pti->m_prev = nullptr;
						pti->m_next = nullptr;

						pti->m_isTerminating = true;
					}

					IEvent *terminatedEvent = pti->m_terminatedEvent;

					pti->m_wakeEvent->Signal();

					// pti may not be valid at this point
					terminatedEvent->Wait();
				}
			}
		}
	}

	Result JobQueue::CreateJob(RCPtr<Job> &outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> &dependencies)
	{
		UniquePtr<IJobRunner> jobRunnerTemp(std::move(jobRunner));

		RCPtr<JobImpl> resultJob;
		RKIT_CHECK(NewWithAlloc<JobImpl>(resultJob, m_alloc, *this, std::move(jobRunnerTemp), dependencies.Count(), jobType));

		CategoryInfo &ci = m_categories[static_cast<size_t>(jobType)];

		{
			MutexLock lock(*m_depGraphMutex);

			if (ci.m_isClosing)
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
		CategoryInfo &ci = m_categories[jobTypeIndex];
		CategoryInfo &allCI = m_categories[static_cast<size_t>(JobType::kCount)];

		CategoryInfo *ciThreadToKick = nullptr;

		MutexLock lock(*m_distributorMutex);
		if (ci.m_firstWaitingThread)
			ciThreadToKick = &ci;
		else if (allCI.m_firstWaitingThread)
			ciThreadToKick = &allCI;

		if (ciThreadToKick != nullptr)
		{
			ParticipatingThreadInfo *pti = ciThreadToKick->m_firstWaitingThread;

			ciThreadToKick->m_firstWaitingThread = pti->m_next;
			if (!ciThreadToKick->m_firstWaitingThread)
				ciThreadToKick->m_lastWaitingThread = nullptr;

			pti->m_next = nullptr;
			pti->m_prev = nullptr;

			pti->m_job = job;

			pti->m_wakeEvent->Signal();

			return;
		}

		// Couldn't find a thread to give this job to, put it in the queue
		if (ci.m_isClosing)
			return;

		job->m_prevRunnableJob = ci.m_lastJob;
		job->m_nextRunnableJob = nullptr;

		if (ci.m_lastJob)
			ci.m_lastJob->m_nextRunnableJob = job;
		else
			ci.m_firstJob = job;

		ci.m_lastJob = job;
	}

	void JobQueue::JobDone(JobImpl *job)
	{
		if (job->m_numStaticDownstream == 0)
			return;

		MutexLock lock(*m_depGraphMutex);

		for (size_t i = 0; i < job->m_numStaticDownstream; i++)
			DependencyDone(job->m_staticDownstreamList[i]);

		for (const RCPtr<JobImpl> &dep : job->m_dynamicDownstream)
			DependencyDone(dep);
	}

	void JobQueue::DependencyDone(const RCPtr<JobImpl> &job)
	{
		job->m_numWaitingDependencies--;
		if (job->m_numWaitingDependencies == 0)
			AddRunnableJob(job, static_cast<size_t>(job->m_jobType));
	}

	RCPtr<Job> JobQueue::WaitForWork(JobType jobType, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent)
	{
		ParticipatingThreadInfo pti;
		pti.m_wakeEvent = wakeEvent;
		pti.m_terminatedEvent = terminatedEvent;

		Span<CategoryInfo> runnableCategories = (jobType == JobType::kAnyJob) ? (m_categories.ToSpan()) : Span<CategoryInfo>(&m_categories[static_cast<size_t>(jobType)], 1);

		for (;;)
		{
			RCPtr<JobImpl> job;

			MutexLock lock(*m_distributorMutex);

			for (CategoryInfo &jobList : runnableCategories)
			{
				if (jobList.m_isClosing)
					return RCPtr<Job>();

				job = jobList.m_firstJob;

				if (job)
				{
					JobImpl *nextJob = job->m_nextRunnableJob;
					jobList.m_firstJob = nextJob;
					if (!jobList.m_firstJob)
						jobList.m_lastJob = nullptr;

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

			const size_t threadCIIndex = (jobType == JobType::kAnyJob) ? static_cast<size_t>(JobType::kCount) : static_cast<size_t>(jobType);

			CategoryInfo &threadCI = m_categories[threadCIIndex];
			{
				MutexLock lock(*m_distributorMutex);

				if (threadCI.m_isClosing)
					break;

				pti.m_prev = threadCI.m_lastWaitingThread;
				pti.m_next = nullptr;

				if (threadCI.m_lastWaitingThread)
					threadCI.m_lastWaitingThread->m_next = &pti;

				threadCI.m_lastWaitingThread = &pti;
			}

			pti.m_wakeEvent->Wait();

			if (pti.m_isTerminating)
				break;

			if (pti.m_job.IsValid())
			{
				RCPtr<Job> job = std::move(pti.m_job);
				pti.m_job.Reset();

				return job;
			}
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
		RKIT_CHECK(sysDriver->CreateMutex(m_distributorMutex));

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
