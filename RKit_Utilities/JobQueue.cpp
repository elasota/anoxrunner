#include "rkit/Core/Drivers.h"
#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Pair.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/StaticBoolArray.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "JobQueue.h"

namespace rkit { namespace utils
{
	class JobQueue;

	struct JobQueueWaitingThreadInfo;

	struct JobQueueWaitRingEntry
	{
		JobQueueWaitRingEntry();

		void Append(JobQueueWaitRingEntry &other);
		void Unlink();
		bool IsIsolated() const;

		JobQueueWaitingThreadInfo *m_owner = nullptr;
		JobQueueWaitRingEntry *m_next = nullptr;
		JobQueueWaitRingEntry *m_prev = nullptr;
	};

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

		// Sync with dep graph mutex
		bool m_dgJobCompleted = false;	// A job is "completed" when it succeeds OR fails
		bool m_dgJobFailed = false;
		bool m_dgJobDependencyFailed = false;

		// Threads waiting for the job to start or complete

		// Sync with distribution mutex
		bool m_distJobWasQueued = false;
		bool m_distJobWasStarted = false;
		JobQueueWaitRingEntry m_distWaitingThreadsRing;
	};

	class JobSignalerImpl final : public JobSignaler
	{
	public:
		JobSignalerImpl(JobQueue &jobQueue, const RCPtr<JobImpl> &job);
		~JobSignalerImpl();

	protected:
		void SignalDoneImpl(PackedResultAndExtCode result) override;

	private:
		// Needed due to vtable being trashed in destructor
		void InternalSignalDone(PackedResultAndExtCode result);

		bool m_haveSignaled = false;

		JobQueue &m_jobQueue;
		RCPtr<JobImpl> m_job;
	};

	struct PendingJobList
	{
		RCPtr<JobImpl> m_firstJob;
		JobImpl *m_lastJob = nullptr;
	};

	enum class JobQueueWakeDisposition
	{
		kInvalid,

		// Dispositions to threads waiting for work:
		kTerminated,

		// Thread pool was closed
		kDequeuedWork,

		// Dispositions to threads waiting for start:

		// Job was completed by another thread, or failed
		kJobCompletedByAnotherThread,
		// Job can be started by this thread (no dependencies failed)
		kJobStartedByThisThread,
	};

	struct JobQueueWaitingThreadInfo
	{
		JobQueueWaitingThreadInfo();

		static const size_t kNumJobCategories = static_cast<size_t>(rkit::JobType::kCount) + 1;

		JobQueueWaitRingEntry m_catWait;
		JobQueueWaitRingEntry m_jobWait;

		RCPtr<JobImpl> m_job;

		IEvent *m_wakeEvent = nullptr;
		IEvent *m_terminatedEvent = nullptr;

		JobQueueWakeDisposition m_wakeDisposition = JobQueueWakeDisposition::kInvalid;

		size_t m_waitListIndex = 0;

		rkit::StaticBoolArray<kNumJobCategories> m_respondsToCategories;

		void Wake(JobQueueWakeDisposition disposition);
		JobQueueWakeDisposition AwaitWake() const;
	};

	class JobQueue final : public IJobQueue
	{
	public:
		friend class JobImpl;
		friend class JobSignalerImpl;

		explicit JobQueue(IMallocDriver *alloc);
		~JobQueue();

		Result CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const JobDependencyList &dependencies) override;

		Result CreateSignaledJob(RCPtr<JobSignaler> &outSignaler, RCPtr<Job> &outJob) override;
		Result CreateSignalJobRunner(UniquePtr<IJobRunner> &outJobRunner, const RCPtr<JobSignaler> &signaller) override;

		// Wait for work or for a specific job.
		// wakeEvent: Event to use for waking up the thread
		// terminatedEvent: Event to be signalled by the waiting thread if it is awoken while the job queue is shutting down.
		void WaitForJob(Job &job, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent) override;

		RCPtr<Job> WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent) override;

		void Fault(const PackedResultAndExtCode &result) override;

		PackedResultAndExtCode CheckFault() override;
		PackedResultAndExtCode Close() override;

		Result Init();

	private:
		enum class WaitResultType
		{
			kNoWork,							// No work is available
			kSpecifiedJobSucceeded,				// Specified job completed successfully
			kSpecifiedJobFailed,				// Specified job failed
			kSpecifiedJobReturned,				// Specified job is runnable, return value is the job
			kTerminated,						// Thread pool is shutting down, no job returned
			kQueuedJob,							// Returned a queued job
		};

		class SignalJobRunner final : public IJobRunner
		{
		public:
			explicit SignalJobRunner(const RCPtr<JobSignaler> &signaller);

			Result Run() override;

		private:
			RCPtr<JobSignaler> m_signaller;
		};

		Pair<RCPtr<Job>, WaitResultType> WaitForWorkOrJob(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent, JobImpl *jobToWaitFor);

		void AddRunnableJob(const RCPtr<JobImpl> &job, JobType jobType);
		void JobDone(JobImpl *job, bool succeeded);
		void DependencyDone(MutexLock &depGraphLock, const RCPtr<JobImpl> &job, bool succeeded, RCPtr<JobImpl> &newlyRunnableJobsPtr);

		struct CategoryThreadWaitList
		{
			JobQueueWaitRingEntry m_waitRing;
		};

		void PrivClose();

		void UnlinkWaitingThread(JobQueueWaitingThreadInfo &wti);

		// Dep graph mutex may be locked within distributor mutex, the opposite must not occur.
		// Distributor mutex is responsible for changing waiting thread lists and job queues.
		// Dep graph mutex is responsible for changing the dependency lists of a job.
		UniquePtr<IMutex> m_distributorMutex;
		UniquePtr<IMutex> m_depGraphMutex;
		UniquePtr<IMutex> m_resultMutex;

		IMallocDriver *m_alloc;
		PackedResultAndExtCode m_result;

		StaticArray<PendingJobList, static_cast<size_t>(JobType::kCount)> m_pendingJobLists;

		StaticArray<CategoryThreadWaitList, JobQueueWaitingThreadInfo::kNumJobCategories> m_singleCategoryThreadWaitLists;
		CategoryThreadWaitList m_multiCategoryThreadWaitList;

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
			RKIT_RETURN_OK;
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
		RKIT_ASSERT(m_numWaitingDependencies == 0);
		RKIT_ASSERT(m_distJobWasStarted == true);

		bool jobSucceeded = true;

		if (m_dgJobDependencyFailed)
		{
			jobSucceeded = false;
			m_jobRunner.Reset();
		}
		else
		{
			if (m_jobRunner.IsValid())
			{
				PackedResultAndExtCode result = RKIT_TRY_EVAL(m_jobRunner->Run());

				jobSucceeded = utils::ResultIsOK(result);
				if (!jobSucceeded)
					m_jobQueue.Fault(result);

				m_jobRunner.Reset();
			}
			else
				jobSucceeded = true;
		}

		m_jobQueue.JobDone(this, jobSucceeded);
	}

	JobSignalerImpl::JobSignalerImpl(JobQueue &jobQueue, const RCPtr<JobImpl> &job)
		: m_jobQueue(jobQueue)
		, m_job(job)
	{
	}

	JobSignalerImpl::~JobSignalerImpl()
	{
		// DO NOT call SignalDone or SignalDoneImpl, since SignalDoneImpl is virtual and not valid here
		if (!m_haveSignaled)
			InternalSignalDone(utils::PackResult(ResultCode::kJobAborted));
	}

	void JobSignalerImpl::SignalDoneImpl(PackedResultAndExtCode result)
	{
		RKIT_ASSERT(m_haveSignaled == false);

		m_haveSignaled = true;

		InternalSignalDone(result);
	}

	void JobSignalerImpl::InternalSignalDone(PackedResultAndExtCode result)
	{
		const bool succeeded = utils::ResultIsOK(result);
		if (!succeeded)
			m_jobQueue.Fault(result);

		m_jobQueue.JobDone(m_job.Get(), succeeded);
	}

	JobQueueWaitRingEntry::JobQueueWaitRingEntry()
		: m_next(this)
		, m_prev(this)
	{
	}

	void JobQueueWaitRingEntry::Append(JobQueueWaitRingEntry &other)
	{
		JobQueueWaitRingEntry *thisStart = this;
		JobQueueWaitRingEntry *thisEnd = this->m_prev;

		JobQueueWaitRingEntry *otherStart = &other;
		JobQueueWaitRingEntry *otherEnd = other.m_prev;

		thisStart->m_prev = otherEnd;
		thisEnd->m_next = otherStart;

		otherStart->m_prev = thisEnd;
		otherEnd->m_next = thisStart;
	}

	void JobQueueWaitRingEntry::Unlink()
	{
		if (!IsIsolated())
		{
			JobQueueWaitRingEntry *outerStart = m_next;
			JobQueueWaitRingEntry *outerEnd = m_prev;

			outerStart->m_prev = outerEnd;
			outerEnd->m_next = outerStart;

			m_next = this;
			m_prev = this;
		}
	}

	bool JobQueueWaitRingEntry::IsIsolated() const
	{
		return m_next == this;
	}

	JobQueueWaitingThreadInfo::JobQueueWaitingThreadInfo()
	{
		m_catWait.m_owner = this;
		m_jobWait.m_owner = this;
	}

	void JobQueueWaitingThreadInfo::Wake(JobQueueWakeDisposition disposition)
	{
		m_wakeDisposition = disposition;
		m_wakeEvent->Signal();
	}

	JobQueueWakeDisposition JobQueueWaitingThreadInfo::AwaitWake() const
	{
		m_wakeEvent->Wait();
		return m_wakeDisposition;
	}

	JobQueue::SignalJobRunner::SignalJobRunner(const RCPtr<JobSignaler> &signaller)
		: m_signaller(signaller)
	{
		RKIT_ASSERT(signaller.IsValid());
	}

	Result JobQueue::SignalJobRunner::Run()
	{
		m_signaller->SignalDone(ResultCode::kOK);
		RKIT_RETURN_OK;
	}

	JobQueue::JobQueue(IMallocDriver *alloc)
		: m_alloc(alloc)
		, m_result(utils::PackResult(ResultCode::kOK))
		, m_isInitialized(false)
	{
	}

	JobQueue::~JobQueue()
	{
		if (m_isInitialized)
		{
			for (const PendingJobList &catInfo : m_pendingJobLists)
			{
				RKIT_ASSERT(catInfo.m_firstJob == nullptr);
			}
			for (const CategoryThreadWaitList &catWaitList : m_singleCategoryThreadWaitLists)
			{
				RKIT_ASSERT(catWaitList.m_waitRing.IsIsolated());
			}
			RKIT_ASSERT(m_multiCategoryThreadWaitList.m_waitRing.IsIsolated());
		}
	}

	void JobQueue::PrivClose()
	{
		if (m_isInitialized)
		{
			// Abort all waiting jobs
			{
				m_isClosing = true;

				bool dequeuedAny = true;
				while (dequeuedAny)
				{
					dequeuedAny = false;

					for (PendingJobList &ci : m_pendingJobLists)
					{
						for (;;)
						{
							RCPtr<JobImpl> job;
							{
								MutexLock lock(*m_distributorMutex);
								if (!ci.m_firstJob.IsValid())
									break;

								job = ci.m_firstJob;
								ci.m_firstJob = job->m_nextRunnableJob;
							}

							dequeuedAny = true;

							JobDone(job.Get(), false);
						}
					}
				}
			}

			// Terminate all waiting threads
			StaticArray<Span<CategoryThreadWaitList>, 2> waitListGroups;
			waitListGroups[0] = m_singleCategoryThreadWaitLists.ToSpan();
			waitListGroups[1] = Span<CategoryThreadWaitList>(&m_multiCategoryThreadWaitList, 1);

			for (const Span<CategoryThreadWaitList> &waitListGroup : waitListGroups)
			{
				for (CategoryThreadWaitList &cwi : waitListGroup)
				{
					for (;;)
					{
						JobQueueWaitRingEntry *wre = nullptr;

						{
							MutexLock lock(*m_distributorMutex);

							wre = cwi.m_waitRing.m_next;

							if (wre == &cwi.m_waitRing)
								break;

							UnlinkWaitingThread(*wre->m_owner);
						}

						// We need to read terminatedEvent since it may not be valid after wakeEvent
						// is signalled
						JobQueueWaitingThreadInfo *wti = wre->m_owner;

						IEvent *terminatedEvent = wti->m_terminatedEvent;

						wti->Wake(JobQueueWakeDisposition::kTerminated);

						terminatedEvent->Wait();
					}
				}
			}
		}
	}

	void JobQueue::UnlinkWaitingThread(JobQueueWaitingThreadInfo &wti)
	{
		wti.m_catWait.Unlink();
		wti.m_jobWait.Unlink();
	}

	Result JobQueue::CreateJob(RCPtr<Job> *outJob, JobType jobType, UniquePtr<IJobRunner> &&jobRunner, const JobDependencyList &dependencies)
	{
		UniquePtr<IJobRunner> jobRunnerTemp(std::move(jobRunner));

		size_t numDependencies = dependencies.GetSpan().Count();

		RCPtr<JobImpl> resultJob;
		RKIT_CHECK(NewWithAlloc<JobImpl>(resultJob, m_alloc, *this, std::move(jobRunnerTemp), numDependencies, jobType));

		PendingJobList &ci = m_pendingJobLists[static_cast<size_t>(jobType)];

		bool isAborted = false;
		if (numDependencies > 0)
		{
			MutexLock lock(*m_depGraphMutex);

			for (Job *job : dependencies.GetSpan())
			{
				RKIT_ASSERT(job != nullptr);

				if (static_cast<JobImpl *>(job)->m_dgJobCompleted)
					resultJob->m_numWaitingDependencies--;

				if (static_cast<JobImpl *>(job)->m_dgJobFailed)
				{
					resultJob->m_numWaitingDependencies = 0;
					isAborted = true;
					break;
				}
			}

			if (!isAborted)
			{
				for (Job *job : dependencies.GetSpan())
				{
					if (!static_cast<JobImpl *>(job)->m_dgJobCompleted)
						RKIT_CHECK(static_cast<JobImpl *>(job)->ReserveDownstreamDependency());
				}

				for (Job *job : dependencies.GetSpan())
				{
					if (!static_cast<JobImpl *>(job)->m_dgJobCompleted)
						*(static_cast<JobImpl *>(job)->GetLastDownstreamDependency()) = resultJob;
				}
			}
		}

		// We can do this without a dependency graph lock because it's not externally visible yet
		if (isAborted)
		{
			resultJob->m_dgJobCompleted = true;
			resultJob->m_dgJobFailed = true;
		}

		if (resultJob->m_numWaitingDependencies == 0)
			AddRunnableJob(resultJob, jobType);

		if (outJob)
			*outJob = std::move(resultJob);

		RKIT_RETURN_OK;
	}

	Result JobQueue::CreateSignaledJob(RCPtr<JobSignaler> &outSignaler, RCPtr<Job> &outJob)
	{
		RCPtr<JobImpl> resultJob;
		RKIT_CHECK(NewWithAlloc<JobImpl>(resultJob, m_alloc, *this, UniquePtr<IJobRunner>(), 0, JobType::kNormalPriority));

		RCPtr<JobSignalerImpl> signaller;
		RKIT_CHECK(New<JobSignalerImpl>(signaller, *this, resultJob));

		outSignaler = std::move(signaller);
		outJob = std::move(resultJob);

		RKIT_RETURN_OK;
	}

	Result JobQueue::CreateSignalJobRunner(UniquePtr<IJobRunner> &outJobRunner, const RCPtr<JobSignaler> &signaller)
	{
		return New<SignalJobRunner>(outJobRunner, signaller);
	}

	void JobQueue::AddRunnableJob(const RCPtr<JobImpl> &job, JobType jobType)
	{
		const size_t jobTypeIndex = static_cast<size_t>(jobType);

		CategoryThreadWaitList *ciWaitListToKick = nullptr;
		JobQueueWaitingThreadInfo *threadToKick = nullptr;

		MutexLock lock(*m_distributorMutex);

		JobImpl *jobPtr = job.Get();

		jobPtr->m_distJobWasQueued = true;

		// OK to check JobDependencyFailed outside of dep graph mutex because it
		if (jobPtr->m_dgJobDependencyFailed || m_isClosing)
		{
			lock.Unlock();

			jobPtr->m_distJobWasStarted = true;

			JobDone(jobPtr, false);
			return;
		}

		// If there are threads waiting for this job to start or complete,
		// give it to the first queued thread, and the rest will wait for it
		// to complete.
		if (!jobPtr->m_distWaitingThreadsRing.IsIsolated())
		{
			JobQueueWaitingThreadInfo *wti = job->m_distWaitingThreadsRing.m_next->m_owner;
			UnlinkWaitingThread(*wti);

			jobPtr->m_distJobWasStarted = true;
			wti->m_job = job;
			wti->Wake(JobQueueWakeDisposition::kJobStartedByThisThread);

			return;
		}

		// Find a thread waiting for this job category
		const size_t kMaxCategoriesToScan = JobQueueWaitingThreadInfo::kNumJobCategories - 1;

		const size_t kThisCategoryBit = (static_cast<size_t>(1) << jobTypeIndex);
		const size_t kLowerCategoriesBits = kThisCategoryBit - 1;

		// Look for a specialized thread
		{
			CategoryThreadWaitList &waitList = m_singleCategoryThreadWaitLists[jobTypeIndex];

			if (!waitList.m_waitRing.IsIsolated())
			{
				threadToKick = waitList.m_waitRing.m_next->m_owner;
				ciWaitListToKick = &waitList;
			}
		}

		// Look for a non-specialized thread
		if (!ciWaitListToKick)
		{
			for (JobQueueWaitRingEntry *wre = m_multiCategoryThreadWaitList.m_waitRing.m_next; wre != &m_multiCategoryThreadWaitList.m_waitRing; wre = wre->m_next)
			{
				if (wre->m_owner->m_respondsToCategories.Get(jobTypeIndex))
				{
					threadToKick = wre->m_owner;
					ciWaitListToKick = &m_multiCategoryThreadWaitList;
					break;
				}
			}
		}

		if (threadToKick != nullptr)
		{
			// Found a thread to wake up
			jobPtr->m_distJobWasStarted = true;

			UnlinkWaitingThread(*threadToKick);

			threadToKick->m_job = jobPtr;
			threadToKick->Wake(JobQueueWakeDisposition::kDequeuedWork);

			return;
		}

		// Couldn't find a thread to give this job to, put it in the queue
		PendingJobList &pji = m_pendingJobLists[jobTypeIndex];
		jobPtr->m_prevRunnableJob = pji.m_lastJob;
		jobPtr->m_nextRunnableJob = nullptr;

		if (pji.m_lastJob)
			pji.m_lastJob->m_nextRunnableJob = job;
		else
			pji.m_firstJob = job;

		pji.m_lastJob = jobPtr;
	}

	void JobQueue::JobDone(JobImpl *job, bool succeeded)
	{
		RCPtr<JobImpl> newlyRunnableJobsPtr;

		{
			MutexLock lock(*m_depGraphMutex);

			job->m_dgJobCompleted = true;
			job->m_dgJobFailed = !succeeded;

			// Signal dependencies done first so any further jobs are queued
			if (job->m_numStaticDownstream != 0)
			{
				for (size_t i = 0; i < job->m_numStaticDownstream; i++)
					DependencyDone(lock, job->m_staticDownstreamList[i], succeeded, newlyRunnableJobsPtr);

				for (const RCPtr<JobImpl> &dep : job->m_dynamicDownstream)
					DependencyDone(lock, dep, succeeded, newlyRunnableJobsPtr);
			}
		}

		// Prioritize waking up any threads specifically waiting for this vs. scheduling downstream
		// dependencies
		{
			MutexLock lock(*m_distributorMutex);

			JobQueueWaitRingEntry *wre = job->m_distWaitingThreadsRing.m_next;

			// Wake up any other waiting threads
			while (wre != &job->m_distWaitingThreadsRing)
			{
				JobQueueWaitRingEntry *nextRingEntry = wre->m_next;

				JobQueueWaitingThreadInfo *wti = wre->m_owner;
				UnlinkWaitingThread(*wti);

				wti->Wake(JobQueueWakeDisposition::kJobCompletedByAnotherThread);
				wre = nextRingEntry;
			}
		}

		while (newlyRunnableJobsPtr.IsValid())
		{
			RCPtr<JobImpl> nextJob = std::move(newlyRunnableJobsPtr->m_nextRunnableJob);
			newlyRunnableJobsPtr->m_nextRunnableJob.Reset();

			if (nextJob.IsValid())
				nextJob->m_prevRunnableJob = nullptr;

			AddRunnableJob(newlyRunnableJobsPtr, newlyRunnableJobsPtr->m_jobType);
			newlyRunnableJobsPtr = nextJob;
		}
	}

	void JobQueue::DependencyDone(MutexLock &depGraphLock, const RCPtr<JobImpl> &job, bool succeeded, RCPtr<JobImpl> &newlyRunnableJobsPtr)
	{
		// depGraphLock is not used, it's just there to enforce that the lock is held
		// when calling this function!
		(void)depGraphLock;

		// We need to keep a list of newly-runnable jobs outside of this function because we expect the dep graph
		// to be locked here, and we don't want to lock the distributor mutex here by calling AddRunnableJob!

		if (!succeeded)
			job->m_dgJobDependencyFailed = true;

		job->m_numWaitingDependencies--;
		if (job->m_numWaitingDependencies == 0)
		{
			if (newlyRunnableJobsPtr.IsValid())
				newlyRunnableJobsPtr->m_prevRunnableJob = job.Get();

			job->m_nextRunnableJob = newlyRunnableJobsPtr;
			newlyRunnableJobsPtr = job;
		}
	}

	void JobQueue::WaitForJob(Job &jobBase, const ISpan<JobType> &idleJobTypes, IEvent *wakeEvent, IEvent *terminatedEvent)
	{
		JobImpl &job = static_cast<JobImpl &>(jobBase);

		for (;;)
		{
			Pair<rkit::RCPtr<Job>, WaitResultType> waitResult = WaitForWorkOrJob(idleJobTypes, true, wakeEvent, terminatedEvent, &job);

			switch (waitResult.GetAt<1>())
			{
			case WaitResultType::kSpecifiedJobSucceeded:
			case WaitResultType::kSpecifiedJobFailed:
			case WaitResultType::kTerminated:
				return;
			case WaitResultType::kSpecifiedJobReturned:
				// Run the job and return
				waitResult.GetAt<0>()->Run();
				return;
			case WaitResultType::kQueuedJob:
				// Run the job and loop
				waitResult.GetAt<0>()->Run();
				break;
			default:
				RKIT_ASSERT(false);
				return;
			}
		}
	}

	RCPtr<Job> JobQueue::WaitForWork(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent)
	{
		Pair<rkit::RCPtr<Job>, WaitResultType> waitResult = WaitForWorkOrJob(jobTypes, waitIfDepleted, wakeEvent, terminatedEvent, nullptr);

		switch (waitResult.GetAt<1>())
		{
		case WaitResultType::kNoWork:
			RKIT_ASSERT(waitIfDepleted == false);
			break;
		case WaitResultType::kTerminated:
		case WaitResultType::kQueuedJob:
			break;
		default:
			RKIT_ASSERT(false);
			break;
		};

		return waitResult.GetAt<0>();
	}

	Pair<RCPtr<Job>, JobQueue::WaitResultType> JobQueue::WaitForWorkOrJob(const ISpan<JobType> &jobTypes, bool waitIfDepleted, IEvent *wakeEvent, IEvent *terminatedEvent, JobImpl *jobToWaitFor)
	{
		JobQueueWaitingThreadInfo wti = {};
		wti.m_wakeEvent = wakeEvent;
		wti.m_terminatedEvent = terminatedEvent;

		if (jobTypes.Count() == 0 && jobToWaitFor == nullptr)
			return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kNoWork);

		StaticBoolArray<JobQueueWaitingThreadInfo::kNumJobCategories> waitListMask;

		for (;;)
		{
			RCPtr<JobImpl> job;

			{
				MutexLock distLock(*m_distributorMutex);

				if (m_isClosing)
					return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kTerminated);

				// If there is a specified job to wait for, see if we can wait for it or run it
				if (jobToWaitFor != nullptr)
				{
					MutexLock dgLock(*m_depGraphMutex);

					if (jobToWaitFor->m_dgJobCompleted)
					{
						// Job to wait for already completed
						if (jobToWaitFor->m_dgJobFailed)
							return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kSpecifiedJobFailed);
						else
							return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kSpecifiedJobSucceeded);
					}
					else if (jobToWaitFor->m_distJobWasStarted)
					{
						// Job to wait for is running, wait for completion
						job = RCPtr<JobImpl>(jobToWaitFor);

						wti.m_wakeEvent = wakeEvent;

						jobToWaitFor->m_distWaitingThreadsRing.Append(wti.m_jobWait);

						dgLock.Unlock();
						distLock.Unlock();

						JobQueueWakeDisposition wakeDisposition = wti.AwaitWake();

						RKIT_ASSERT(wti.m_catWait.IsIsolated());

						if (wakeDisposition == JobQueueWakeDisposition::kTerminated)
						{
							if (terminatedEvent)
								terminatedEvent->Signal();

							return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kTerminated);
						}

						RKIT_ASSERT(wakeDisposition == JobQueueWakeDisposition::kJobCompletedByAnotherThread);

						if (jobToWaitFor->m_dgJobFailed)
							return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kSpecifiedJobFailed);
						else
							return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kSpecifiedJobSucceeded);
					}
					else
					{
						dgLock.Unlock();

						if (jobToWaitFor->m_distJobWasQueued)
						{
							// Job to wait for is available to run.  Keep an RCPtr here so it survives unlinking.
							job = RCPtr<JobImpl>(jobToWaitFor);

							// Unlink from the pending job list.  Need to be careful here since the next-job and first-job
							// references are RCPtr but the last-job and prev-job references are not!
							PendingJobList &pendingJobList = m_pendingJobLists[static_cast<size_t>(jobToWaitFor->m_jobType)];
							if (pendingJobList.m_firstJob == jobToWaitFor)
								pendingJobList.m_firstJob = jobToWaitFor->m_nextRunnableJob;
							if (pendingJobList.m_lastJob == jobToWaitFor)
								pendingJobList.m_lastJob = jobToWaitFor->m_prevRunnableJob;

							if (jobToWaitFor->m_prevRunnableJob)
								jobToWaitFor->m_prevRunnableJob->m_nextRunnableJob = jobToWaitFor->m_nextRunnableJob;
							if (jobToWaitFor->m_nextRunnableJob)
								jobToWaitFor->m_nextRunnableJob->m_prevRunnableJob = jobToWaitFor->m_prevRunnableJob;

							jobToWaitFor->m_nextRunnableJob.Reset();
							jobToWaitFor->m_prevRunnableJob = nullptr;

							jobToWaitFor->m_distJobWasStarted = true;

							return Pair<RCPtr<Job>, JobQueue::WaitResultType>(job, WaitResultType::kSpecifiedJobReturned);
						}
					}

					// ... otherwise, the specified job is still waiting for dependencies, fall back
					// to running other jobs.
				}

				// Look for a job to run
				for (JobType jobType : jobTypes)
				{
					const size_t jobTypeInt = static_cast<size_t>(jobType);

					job = m_pendingJobLists[jobTypeInt].m_firstJob;

					if (job.IsValid())
					{
						RCPtr<JobImpl> nextJob = job->m_nextRunnableJob;

						PendingJobList &ci = m_pendingJobLists[jobTypeInt];

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
					// Found something to run
					job->m_nextRunnableJob = nullptr;
					job->m_prevRunnableJob = nullptr;

					job->m_distJobWasStarted = true;

					return Pair<RCPtr<Job>, JobQueue::WaitResultType>(job, WaitResultType::kQueuedJob);
				}

				if (!waitIfDepleted)
					return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kNoWork);

				// Couldn't find anything to run, start waiting
				CategoryThreadWaitList *waitList = nullptr;

				if (jobTypes.Count() == 1)
					waitList = &m_singleCategoryThreadWaitLists[static_cast<size_t>(jobTypes[0])];
				else if (jobTypes.Count() > 1)
				{
					waitList = &m_multiCategoryThreadWaitList;
					wti.m_respondsToCategories = waitListMask;
				}

				if (waitList)
					waitList->m_waitRing.Append(wti.m_catWait);

				// If waiting for a specific job, register it in the wait start list
				if (jobToWaitFor)
					jobToWaitFor->m_distWaitingThreadsRing.Append(wti.m_jobWait);

				if (waitList == nullptr && jobToWaitFor == nullptr)
				{
					// Didn't wait for anything?
					RKIT_ASSERT(false);
					return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kNoWork);
				}

				// Unlock of distribution mutex happens here
			}

			JobQueueWakeDisposition wakeDisposition = wti.AwaitWake();

			switch (wakeDisposition)
			{
			case JobQueueWakeDisposition::kTerminated:
				if (terminatedEvent)
					terminatedEvent->Signal();

				return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kTerminated);
			case JobQueueWakeDisposition::kDequeuedWork:
				return Pair<RCPtr<Job>, JobQueue::WaitResultType>(wti.m_job, WaitResultType::kQueuedJob);
			case JobQueueWakeDisposition::kJobStartedByThisThread:
				return Pair<RCPtr<Job>, JobQueue::WaitResultType>(wti.m_job, WaitResultType::kSpecifiedJobReturned);
			case JobQueueWakeDisposition::kJobCompletedByAnotherThread:
				RKIT_ASSERT(jobToWaitFor->m_dgJobCompleted);
				if (jobToWaitFor->m_dgJobFailed)
					return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kSpecifiedJobFailed);
				else
					return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kSpecifiedJobSucceeded);
			default:
				RKIT_ASSERT(false);
				return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kTerminated);
			};
		}

		RKIT_ASSERT(false);

		return Pair<RCPtr<Job>, JobQueue::WaitResultType>(RCPtr<Job>(), WaitResultType::kNoWork);
	}

	void JobQueue::Fault(const PackedResultAndExtCode &result)
	{
		MutexLock lock(*m_resultMutex);
		if (!utils::ResultIsOK(result) && utils::ResultIsOK(m_result))
			m_result = result;
	}

	PackedResultAndExtCode JobQueue::CheckFault()
	{
		PackedResultAndExtCode result;

		{
			MutexLock lock(*m_resultMutex);
			result = m_result;
		}

		return result;
	}

	PackedResultAndExtCode JobQueue::Close()
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

		RKIT_RETURN_OK;
	}
} } // rkit::utils

rkit::Result rkit::utils::CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc)
{
	UniquePtr<JobQueue> jobQueue;
	RKIT_CHECK(NewWithAlloc<JobQueue>(jobQueue, alloc, alloc));
	RKIT_CHECK(jobQueue->Init());

	outJobQueue = std::move(jobQueue);

	RKIT_RETURN_OK;
}
