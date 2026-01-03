#include "AnoxAbstractSingleFileResource.h"

namespace anox
{
	AnoxAbstractSingleFileLoaderAnalyzeJob::AnoxAbstractSingleFileLoaderAnalyzeJob(const rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> &state,
		const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller)
		: m_state(state)
		, m_waitForDependenciesSignaller(waitForDependenciesSignaller)
	{
	}

	rkit::Result AnoxAbstractSingleFileLoaderAnalyzeJob::Run()
	{
		rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> dependencyJobs;

		RKIT_CHECK(m_state->m_functions->m_analyzeFile(*m_state, dependencyJobs));

		if (!dependencyJobs.Count())
		{
			if (m_waitForDependenciesSignaller.IsValid())
				m_waitForDependenciesSignaller->SignalDone(rkit::ResultCode::kOK);
		}
		else
		{
			RKIT_ASSERT(m_waitForDependenciesSignaller.IsValid());

			rkit::IJobQueue &jobQueue = m_state->m_systems.m_fileSystem->GetJobQueue();

			rkit::UniquePtr<rkit::IJobRunner> dependenciesDoneJobRunner;
			RKIT_CHECK(jobQueue.CreateSignalJobRunner(dependenciesDoneJobRunner, m_waitForDependenciesSignaller));
			RKIT_CHECK(jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(dependenciesDoneJobRunner), dependencyJobs.ToSpan()));
		}

		return rkit::ResultCode::kOK;
	}

	AnoxAbstractSingleFileLoaderLoadJob::AnoxAbstractSingleFileLoaderLoadJob(const rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> &state)
		: m_state(state)
	{
	}

	rkit::Result AnoxAbstractSingleFileLoaderLoadJob::Run()
	{
		return m_state->m_functions->m_loadFile(*m_state);
	}
}
