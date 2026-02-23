#include "AnoxAbstractSingleFileResource.h"

namespace anox
{
	AnoxAbstractSingleFileLoaderPhaseJob::AnoxAbstractSingleFileLoaderPhaseJob(const rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> &state,
		size_t phase, const rkit::RCPtr<rkit::JobSignaler> &waitForDependenciesSignaler)
		: m_state(state)
		, m_waitForDependenciesSignaler(waitForDependenciesSignaler)
		, m_phase(phase)
	{
	}

	rkit::Result AnoxAbstractSingleFileLoaderPhaseJob::Run()
	{
		rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> dependencyJobs;

		RKIT_CHECK(m_state->m_functions->m_loadPhaseCallback(*m_state, m_phase, dependencyJobs));

		if (!dependencyJobs.Count())
		{
			if (m_waitForDependenciesSignaler.IsValid())
				m_waitForDependenciesSignaler->SignalDone(rkit::ResultCode::kOK);
		}
		else
		{
			RKIT_ASSERT(m_waitForDependenciesSignaler.IsValid());

			rkit::IJobQueue &jobQueue = m_state->m_systems.m_fileSystem->GetJobQueue();

			rkit::UniquePtr<rkit::IJobRunner> dependenciesDoneJobRunner;
			RKIT_CHECK(jobQueue.CreateSignalJobRunner(dependenciesDoneJobRunner, m_waitForDependenciesSignaler));
			RKIT_CHECK(jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(dependenciesDoneJobRunner), dependencyJobs.ToSpan()));
		}

		RKIT_RETURN_OK;
	}
}
