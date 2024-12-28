#include "rkit/Core/Event.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
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
		JobType GetMainThreadJobType() const override;

	private:
		struct ThreadData
		{
			UniqueThreadRef m_thread;
		};

		Result PrivClose();

		uint32_t m_numThreads;

		const IUtilitiesDriver &m_utils;

		UniquePtr<IJobQueue> m_jobQueue;
		Vector<ThreadData> m_threads;
	};

	class ThreadPoolThreadContext final : public IThreadContext
	{
	public:
		explicit ThreadPoolThreadContext(ThreadPool &pool, JobType jobType, UniquePtr<IEvent> &&wakeEvent, UniquePtr<IEvent> &&terminateEvent);

	private:
		Result Run() override;

		ThreadPool &m_pool;
		JobType m_jobType;

		UniquePtr<IEvent> m_wakeEvent;
		UniquePtr<IEvent> m_terminateEvent;
	};

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

	JobType ThreadPool::GetMainThreadJobType() const
	{
		if (m_numThreads > 0)
			return JobType::kNormalPriority;
		else
			return JobType::kAnyJob;
	}


	ThreadPool::~ThreadPool()
	{
		(void) PrivClose();
	}

	Result ThreadPool::PrivClose()
	{
		Result finalResult;

		if (m_jobQueue.IsValid())
		{
			Result result = m_jobQueue->Close();

			if (!result.IsOK())
				finalResult = result;
		}

		for (ThreadData &threadData : m_threads)
		{
			Result result = threadData.m_thread.Finalize();

			if (!result.IsOK())
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

			ThreadData &td = m_threads[i];

			UniquePtr<IEvent> wakeEvent;
			UniquePtr<IEvent> terminateEvent;

			RKIT_CHECK(sysDriver->CreateEvent(wakeEvent, true, false));
			RKIT_CHECK(sysDriver->CreateEvent(terminateEvent, true, false));

			UniquePtr<IThreadContext> context;
			RKIT_CHECK(New<ThreadPoolThreadContext>(context, *this, jobType, std::move(wakeEvent), std::move(terminateEvent)));

			RKIT_CHECK(GetDrivers().m_systemDriver->CreateThread(td.m_thread, std::move(context)));
		}

		return ResultCode::kOK;
	}

	ThreadPoolThreadContext::ThreadPoolThreadContext(ThreadPool &pool, JobType jobType, UniquePtr<IEvent> &&wakeEvent, UniquePtr<IEvent> &&terminateEvent)
		: m_pool(pool)
		, m_jobType(jobType)
		, m_wakeEvent(std::move(wakeEvent))
		, m_terminateEvent(std::move(terminateEvent))
	{
	}

	Result ThreadPoolThreadContext::Run()
	{
		for (;;)
		{
			RCPtr<Job> job = m_pool.GetJobQueue()->WaitForWork(m_jobType, true, m_wakeEvent.Get(), m_terminateEvent.Get());
			if (!job)
				break;

			job->Run();

			if (!m_pool.GetJobQueue()->CheckFault().IsOK())
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
