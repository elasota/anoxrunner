#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/JobTypeList.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Thread.h"

#include "ThreadPool.h"

namespace rkit::utils
{
	class ThreadPool final : public ThreadPoolBase
	{
	public:
		explicit ThreadPool(const IUtilitiesDriver &utils, uint32_t numThreads);
		~ThreadPool();

		Result Initialize();

		Result Close() override;
		IJobQueue *GetJobQueue() const override;
		const ISpan<JobType> &GetMainThreadJobTypes() const override;
		const ISpan<JobType> &GetAllJobTypes() const override;

	private:
		struct ThreadData
		{
			UniqueThreadRef m_thread;
		};

		struct AllJobTypesSpan : public ISpan<JobType>
		{
			size_t Count() const override;
			JobType operator[](size_t index) const override;
		};

		Result PrivClose();

		uint32_t m_numThreads;

		const IUtilitiesDriver &m_utils;

		UniquePtr<IJobQueue> m_jobQueue;
		Vector<ThreadData> m_threads;

		AllJobTypesSpan m_allJobsSpan;
		JobTypeList<JobType::kNormalPriority> m_normalJobsSpan;
	};

	class ThreadPoolThreadContext final : public IThreadContext
	{
	public:
		explicit ThreadPoolThreadContext(ThreadPool &pool, Vector<JobType> &&jobTypes, UniquePtr<IEvent> &&wakeEvent, UniquePtr<IEvent> &&terminateEvent);

	private:
		Result Run() override;

		ThreadPool &m_pool;
		Vector<JobType> m_jobTypes;

		UniquePtr<IEvent> m_wakeEvent;
		UniquePtr<IEvent> m_terminateEvent;
	};

	size_t ThreadPool::AllJobTypesSpan::Count() const
	{
		return static_cast<size_t>(JobType::kCount);
	}

	JobType ThreadPool::AllJobTypesSpan::operator[](size_t index) const
	{
		return static_cast<JobType>(index);
	}

	ThreadPool::ThreadPool(const IUtilitiesDriver &utils, uint32_t numThreads)
		: m_numThreads(numThreads)
		, m_utils(utils)
	{
	}

	Result ThreadPool::Close()
	{
		return PrivClose();
	}

	IJobQueue *ThreadPool::GetJobQueue() const
	{
		return m_jobQueue.Get();
	}

	const ISpan<JobType> &ThreadPool::GetMainThreadJobTypes() const
	{
		if (m_numThreads > 0)
			return m_normalJobsSpan;
		else
			return m_allJobsSpan;
	}

	const ISpan<JobType> &ThreadPool::GetAllJobTypes() const
	{
		return m_allJobsSpan;
	}

	ThreadPool::~ThreadPool()
	{
		(void) PrivClose();
	}

	Result ThreadPool::PrivClose()
	{
		Result finalResult = ResultCode::kOK;

		if (m_jobQueue.IsValid())
		{
			Result result = m_jobQueue->Close();

			if (!utils::ResultIsOK(result))
				finalResult = result;
		}

		for (ThreadData &threadData : m_threads)
		{
			Result result = threadData.m_thread.Finalize();

			if (!utils::ResultIsOK(result))
				finalResult = result;
		}

		m_threads.Reset();
		m_jobQueue.Reset();

		return finalResult;
	}

	Result ThreadPool::Initialize()
	{
		ISystemDriver *sysDriver = GetDrivers().m_systemDriver;

		RKIT_CHECK(m_utils.CreateJobQueue(m_jobQueue, GetDrivers().m_mallocDriver));

		RKIT_CHECK(m_threads.Resize(m_numThreads));

		for (uint32_t i = 0; i < m_numThreads; i++)
		{
			JobType jobType = JobType::kNormalPriority;

			if (i == 0)
				jobType = JobType::kIO;

			Vector<JobType> jobTypes;
			RKIT_CHECK(jobTypes.Append(jobType));

			ThreadData &td = m_threads[i];

			UniquePtr<IEvent> wakeEvent;
			UniquePtr<IEvent> terminateEvent;

			RKIT_CHECK(sysDriver->CreateEvent(wakeEvent, true, false));
			RKIT_CHECK(sysDriver->CreateEvent(terminateEvent, true, false));

			UniquePtr<IThreadContext> context;
			RKIT_CHECK(New<ThreadPoolThreadContext>(context, *this, std::move(jobTypes), std::move(wakeEvent), std::move(terminateEvent)));

			RKIT_CHECK(GetDrivers().m_systemDriver->CreateThread(td.m_thread, std::move(context)));
		}

		return ResultCode::kOK;
	}

	ThreadPoolThreadContext::ThreadPoolThreadContext(ThreadPool &pool, Vector<JobType> &&jobTypes, UniquePtr<IEvent> &&wakeEvent, UniquePtr<IEvent> &&terminateEvent)
		: m_pool(pool)
		, m_jobTypes(std::move(jobTypes))
		, m_wakeEvent(std::move(wakeEvent))
		, m_terminateEvent(std::move(terminateEvent))
	{
	}

	Result ThreadPoolThreadContext::Run()
	{
		for (;;)
		{
			RCPtr<Job> job = m_pool.GetJobQueue()->WaitForWork(m_jobTypes.ToSpan().ToValueISpan(), true, m_wakeEvent.Get(), m_terminateEvent.Get());
			if (!job)
				break;

			job->Run();

			if (!utils::ResultIsOK(m_pool.GetJobQueue()->CheckFault()))
				break;
		}

		return ResultCode::kOK;
	}

	Result ThreadPoolBase::Create(UniquePtr<ThreadPoolBase> &outThreadPool, const IUtilitiesDriver &utils, uint32_t numThreads)
	{
		UniquePtr<ThreadPool> threadPool;
		RKIT_CHECK(New<ThreadPool>(threadPool, utils, numThreads));
		RKIT_CHECK(threadPool->Initialize());

		outThreadPool = std::move(threadPool);

		return ResultCode::kOK;
	}
}
