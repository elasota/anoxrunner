#include "rkit/Core/Drivers.h"
#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/StaticBoolVector.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "JobQueue.h"

namespace rkit::utils
{
	class JobQueue;

	struct JobCategoryInfo;

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
		struct WaitingThread
		{
			IEvent *m_event = nullptr;
			WaitingThread *m_prev = nullptr;
			WaitingThread *m_next = nullptr;
		};

		static const size_t kStaticDownstreamListSize = 8;

		size_t m_numWaitingDependencies = 0;

		JobQueue &m_jobQueue;
		UniquePtr<IJobRunner> m_jobRunner;

		StaticArray<RCPtr<JobImpl>, kStaticDownstreamListSize> m_staticDownstreamList;
		size_t m_numStaticDownstream = 0;

		Vector<RCPtr<JobImpl>> m_dynamicDownstream;

		RCPtr<JobImpl> m_nextRunnableJob;
		JobImpl *m_prevRunnableJob = nullptr;

		JobCategoryInfo *m_jobCategoryInfo = nullptr;

		JobType m_jobType;

		// Sync with dep graph mutex
		bool m_jobCompleted = false;	// A job is "completed" when it succeeds OR fails
		bool m_jobFailed = false;
		bool m_jobDependencyFailed = false;

		WaitingThread *m_firstWaitingThread = nullptr;
		WaitingThread *m_lastWaitingThread = nullptr;

		// Sync with distribution mutex
		bool m_jobWasQueued = false;
		bool m_jobWasStarted = false;
		rkit::IEvent *m_alertSpecificThreadEvent = nullptr;
	};


	struct JobCategoryInfo
	{
		RCPtr<JobImpl> m_firstJob;
		JobImpl *m_lastJob = nullptr;
	};

	class JobQueue final : public IJobQueue
	{
	public:
		friend class JobImpl;

		explicit JobQueue(IMallocDriver *alloc);
		~JobQueue();

		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> &dependencies) override;
		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<RCPtr<Job> > &dependencies) override;

		void WaitForJob(Job &job, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent, IEvent *alertSpecificThreadEvent) override;

		RCPtr<Job> WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) override;

		void Fault(const Result &result) override;

		Result CheckFault() override;
		Result Close() override;

		Result Init();

	private:
		static const size_t kNumJobCategories = static_cast<size_t>(rkit::JobType::kCount) + 1;

		void AddRunnableJob(const RCPtr<JobImpl> &job, size_t jobTypeIndex);
		void JobDone(JobImpl *job, bool succeeded);
		void DependencyDone(const RCPtr<JobImpl> &job, bool succeeded);

		struct ParticipatingThreadInfo
		{
			ParticipatingThreadInfo *m_next = nullptr;
			ParticipatingThreadInfo *m_prev = nullptr;

			RCPtr<JobImpl> m_job;

			bool m_isTerminating = false;

			IEvent* m_wakeEvent = nullptr;
			IEvent* m_terminatedEvent = nullptr;

			rkit::StaticBoolVector<kNumJobCategories> m_respondsToCategories;
		};

		struct CategoryWaitList
		{
			ParticipatingThreadInfo *m_firstWaitingThread = nullptr;
			ParticipatingThreadInfo *m_lastWaitingThread = nullptr;
		};

		void PrivClose();

		// Dep graph mutex may be locked within distributor mutex, the opposite must not occur
		UniquePtr<IMutex> m_distributorMutex;
		UniquePtr<IMutex> m_depGraphMutex;
		UniquePtr<IMutex> m_resultMutex;

		IMallocDriver *m_alloc;
		Result m_result;

		StaticArray<JobCategoryInfo, kNumJobCategories> m_categories;
		StaticArray<CategoryWaitList, kNumJobCategories> m_singleCategoryWaitLists;
		CategoryWaitList m_multiCategoryWaitList;

		bool m_isInitialized = false;

		// Protect under distributor mutex
		bool m_isClosing = false;
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
		bool jobSucceeded = true;

		if (m_jobDependencyFailed)
			jobSucceeded = false;
		else
		{
			Result result = m_jobRunner->Run();

			if (!utils::ResultIsOK(result))
				m_jobQueue.Fault(result);

			m_jobRunner.Reset();

			jobSucceeded = utils::ResultIsOK(result);
		}

		m_jobQueue.JobDone(this, jobSucceeded);
	}

	JobQueue::JobQueue(IMallocDriver *alloc)
		: m_alloc(alloc)
		, m_isInitialized(false)
		, m_result(ResultCode::kOK)
	{
	}

	JobQueue::~JobQueue()
	{
		if (m_isInitialized)
		{
			for (const JobCategoryInfo &catInfo : m_categories)
			{
				RKIT_ASSERT(catInfo.m_firstJob == nullptr);
			}
			for (const CategoryWaitList &catWaitList : m_singleCategoryWaitLists)
			{
				RKIT_ASSERT(catWaitList.m_firstWaitingThread == nullptr);
			}
			RKIT_ASSERT(m_multiCategoryWaitList.m_firstWaitingThread == nullptr);
		}
	}

	void JobQueue::PrivClose()
	{
		if (m_isInitialized)
		{
			{
				MutexLock lock(*m_distributorMutex);
				m_isClosing = true;

				for (JobCategoryInfo &ci : m_categories)
				{
					ci.m_firstJob = nullptr;
					ci.m_lastJob = nullptr;
				}
			}

			StaticArray<Span<CategoryWaitList>, 2> waitListGroups;
			waitListGroups[0] = m_singleCategoryWaitLists.ToSpan();
			waitListGroups[1] = Span<CategoryWaitList>(&m_multiCategoryWaitList, 1);

			for (const Span<CategoryWaitList> &waitListGroup : waitListGroups)
			{
				for (CategoryWaitList &cwi : waitListGroup)
				{
					for (;;)
					{
						ParticipatingThreadInfo *pti = nullptr;

						{
							MutexLock lock(*m_distributorMutex);

							pti = cwi.m_firstWaitingThread;

							if (!pti)
								break;

							cwi.m_firstWaitingThread = pti->m_next;
							if (!cwi.m_firstWaitingThread)
								cwi.m_lastWaitingThread = nullptr;

							pti->m_prev = nullptr;
							pti->m_next = nullptr;

							pti->m_isTerminating = true;
						}

						// We need to read terminatedEvent since it may not be valid after wakeEvent
						// is signaled
						IEvent *terminatedEvent = pti->m_terminatedEvent;

						pti->m_wakeEvent->Signal();

						terminatedEvent->Wait();
					}
				}
			}
		}
	}

	Result JobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<Job *> &dependencies)
	{
		UniquePtr<IJobRunner> jobRunnerTemp(std::move(jobRunner));

		size_t numDependencies = dependencies.Count();

		RCPtr<JobImpl> resultJob;
		RKIT_CHECK(NewWithAlloc<JobImpl>(resultJob, m_alloc, *this, std::move(jobRunnerTemp), numDependencies, jobType));

		JobCategoryInfo &ci = m_categories[static_cast<size_t>(jobType)];

		bool isRunnableNow = true;
		bool isAborted = false;
		if (numDependencies > 0)
		{
			MutexLock lock(*m_depGraphMutex);

			for (Job *job : dependencies)
			{
				if (!static_cast<JobImpl *>(job)->m_jobCompleted)
					isRunnableNow = false;

				if (static_cast<JobImpl *>(job)->m_jobFailed)
				{
					isAborted = true;
					isRunnableNow = false;
					break;
				}
			}

			if (!isAborted)
			{
				for (Job *job : dependencies)
				{
					if (!static_cast<JobImpl *>(job)->m_jobCompleted)
					{
						isRunnableNow = false;
						RKIT_CHECK(static_cast<JobImpl *>(job)->ReserveDownstreamDependency());
					}
				}

				for (Job *job : dependencies)
				{
					if (!static_cast<JobImpl *>(job)->m_jobCompleted)
					{
						*(static_cast<JobImpl *>(job)->GetLastDownstreamDependency()) = resultJob;
					}
				}
			}
		}

		if (isAborted)
		{
			resultJob->m_jobCompleted = true;
			resultJob->m_jobFailed = true;
		}

		if (isRunnableNow)
			AddRunnableJob(resultJob, static_cast<size_t>(jobType));

		if (outJob)
			*outJob = std::move(resultJob);

		return ResultCode::kOK;
	}

	Result JobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const ISpan<RCPtr<Job> > &dependencies)
	{
		struct SpanConverter final : public rkit::ISpan<rkit::Job *>
		{
			explicit SpanConverter(const ISpan<RCPtr<Job> > &baseSpan)
				: m_baseSpan(baseSpan)
			{
			}

			size_t Count() const override
			{
				return m_baseSpan.Count();
			}

			rkit::Job *operator[](size_t index) const
			{
				return m_baseSpan[index].Get();
			}

		private:
			const ISpan<RCPtr<Job> > &m_baseSpan;
		};

		SpanConverter spanConverter(dependencies);

		return CreateJob(outJob, jobType, std::move(jobRunner), spanConverter);
	}

	void JobQueue::AddRunnableJob(const RCPtr<JobImpl> &job, size_t jobTypeIndex)
	{
		CategoryWaitList *ciWaitListToKick = nullptr;
		ParticipatingThreadInfo *threadToKick = nullptr;

		MutexLock lock(*m_distributorMutex);

		if (job->m_jobDependencyFailed || m_isClosing)
		{
			lock.Unlock();

			JobDone(job.Get(), false);
			return;
		}

		const size_t kMaxCategoriesToScan = kNumJobCategories - 1;

		const size_t kThisCategoryBit = (static_cast<size_t>(1) << jobTypeIndex);
		const size_t kLowerCategoriesBits = kThisCategoryBit - 1;

		for (CategoryWaitList &waitList : m_singleCategoryWaitLists)
		{
			if (waitList.m_firstWaitingThread)
			{
				threadToKick = waitList.m_firstWaitingThread;
				ciWaitListToKick = &waitList;
				break;
			}
		}

		if (!ciWaitListToKick)
		{
			for (ParticipatingThreadInfo *threadInfo = m_multiCategoryWaitList.m_firstWaitingThread; threadInfo != nullptr; threadInfo = threadInfo->m_next)
			{
				if (threadInfo->m_respondsToCategories.Get(jobTypeIndex))
				{
					threadToKick = threadInfo;
					ciWaitListToKick = &m_multiCategoryWaitList;
					break;
				}
			}
		}

		if (threadToKick != nullptr)
		{
			job->m_jobWasQueued = true;
			job->m_jobWasStarted = true;

			ParticipatingThreadInfo *pti = ciWaitListToKick->m_firstWaitingThread;

			if (threadToKick->m_prev)
				threadToKick->m_prev->m_next = threadToKick->m_next;
			else
				ciWaitListToKick->m_firstWaitingThread = threadToKick->m_next;

			if (threadToKick->m_next)
				threadToKick->m_next->m_prev = threadToKick->m_prev;
			else
				ciWaitListToKick->m_lastWaitingThread = threadToKick->m_prev;

			pti->m_next = nullptr;
			pti->m_prev = nullptr;

			pti->m_job = job;

			pti->m_wakeEvent->Signal();

			return;
		}

		// Couldn't find a thread to give this job to, put it in the queue
		JobCategoryInfo &ci = m_categories[jobTypeIndex];
		job->m_prevRunnableJob = ci.m_lastJob;
		job->m_nextRunnableJob = nullptr;

		if (ci.m_lastJob)
			ci.m_lastJob->m_nextRunnableJob = job;
		else
			ci.m_firstJob = job;

		ci.m_lastJob = job;

		job->m_jobWasQueued = true;
	}

	void JobQueue::JobDone(JobImpl *job, bool succeeded)
	{
		MutexLock lock(*m_depGraphMutex);

		job->m_jobCompleted = true;
		job->m_jobFailed = !succeeded;

		// Signal dependencies done first so any further jobs are queued
		if (job->m_numStaticDownstream != 0)
		{
			for (size_t i = 0; i < job->m_numStaticDownstream; i++)
				DependencyDone(job->m_staticDownstreamList[i], succeeded);

			for (const RCPtr<JobImpl> &dep : job->m_dynamicDownstream)
				DependencyDone(dep, succeeded);
		}

		const JobImpl::WaitingThread *waitingThread = job->m_firstWaitingThread;

		while (waitingThread != nullptr)
		{
			// Get the next one now since waking it will destroy the link
			const JobImpl::WaitingThread *nextThread = waitingThread->m_next;
			waitingThread->m_event->Signal();
			waitingThread = nextThread;
		}
	}

	void JobQueue::DependencyDone(const RCPtr<JobImpl> &job, bool succeeded)
	{
		if (!succeeded)
			job->m_jobDependencyFailed = true;

		job->m_numWaitingDependencies--;
		if (job->m_numWaitingDependencies == 0)
			AddRunnableJob(job, static_cast<size_t>(job->m_jobType));
	}

	void JobQueue::WaitForJob(Job &jobBase, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent, IEvent *alertSpecificThreadEvent)
	{
		JobImpl &job = static_cast<JobImpl &>(jobBase);

		for (;;)
		{
			// Do any work scheduled for this thread
			rkit::RCPtr<Job> job = WaitForWork(idleJobTypes, false, wakeEvent, terminatedEvent);

			if (!job.IsValid())
				break;

			job->Run();
		}

		JobImpl::WaitingThread waitingThread;
		waitingThread.m_event = alertSpecificThreadEvent;
		waitingThread.m_next = nullptr;
		waitingThread.m_prev = nullptr;

		bool mustWait = false;
		{
			MutexLock lock(*m_depGraphMutex);
			if (!job.m_jobCompleted)
			{
				mustWait = true;
				if (job.m_lastWaitingThread)
					job.m_lastWaitingThread->m_next = &waitingThread;
				else
					job.m_firstWaitingThread = &waitingThread;

				waitingThread.m_prev = job.m_lastWaitingThread;
				job.m_lastWaitingThread = &waitingThread;
			}
		}

		if (mustWait)
			alertSpecificThreadEvent->Wait();
	}

	RCPtr<Job> JobQueue::WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent)
	{
		ParticipatingThreadInfo pti;
		pti.m_wakeEvent = wakeEvent;
		pti.m_terminatedEvent = terminatedEvent;

		if (jobTypes.Count() == 0)
			return RCPtr<Job>();

		StaticBoolVector<kNumJobCategories> waitListMask;

		for (;;)
		{
			RCPtr<JobImpl> job;

			{
				MutexLock lock(*m_distributorMutex);

				if (m_isClosing)
					return RCPtr<Job>();

				for (JobType jobType : jobTypes)
				{
					const size_t jobTypeInt = static_cast<size_t>(jobType);

					job = m_categories[jobTypeInt].m_firstJob;

					if (job)
					{
						JobImpl *nextJob = job->m_nextRunnableJob;

						JobCategoryInfo &ci = m_categories[jobTypeInt];

						ci.m_firstJob = nextJob;
						if (!ci.m_firstJob)
							ci.m_lastJob = nullptr;

						if (nextJob)
							nextJob->m_prevRunnableJob = nullptr;

						break;
					}

					waitListMask.Set(jobTypeInt, true);
				}

				if (job)
				{
					job->m_nextRunnableJob = nullptr;
					job->m_prevRunnableJob = nullptr;

					job->m_jobWasStarted = true;

					return job;
				}

				if (!waitIfDepleted)
					break;

				CategoryWaitList *waitList = nullptr;

				if (jobTypes.Count() == 1)
					waitList = &m_singleCategoryWaitLists[static_cast<size_t>(jobTypes[0])];
				else
				{
					waitList = &m_multiCategoryWaitList;
					pti.m_respondsToCategories = waitListMask;
				}

				pti.m_prev = waitList->m_lastWaitingThread;
				pti.m_next = nullptr;

				if (waitList->m_lastWaitingThread)
					waitList->m_lastWaitingThread->m_next = &pti;

				waitList->m_lastWaitingThread = &pti;

				if (!waitList->m_firstWaitingThread)
					waitList->m_firstWaitingThread = &pti;
			}

			pti.m_wakeEvent->Wait();

			if (pti.m_isTerminating)
				break;

			if (pti.m_job.IsValid())
			{
				RCPtr<Job> job = std::move(pti.m_job);
				pti.m_job.Reset();

				static_cast<JobImpl *>(job.Get())->m_jobWasStarted = true;

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
		if (!utils::ResultIsOK(result))
			m_result = result;
	}

	Result JobQueue::CheckFault()
	{
		Result result(ResultCode::kOK);

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

		m_isInitialized = true;

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
