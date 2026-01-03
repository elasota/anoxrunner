#pragma once

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"

#include "rkit/Core/HybridVector.h"

#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"
#include "AnoxResourceManager.h"

namespace anox
{
	struct AnoxAbstractSingleFileResourceLoaderState;
}

namespace anox { namespace priv {
	template<class TLoaderInfo, bool THasAnalysisPhase, bool THasDependencies>
	struct SingleFileResourceLoaderAnalysisHelper
	{
	};

	template<class TLoaderInfo, bool THasDependencies>
	struct SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, false, THasDependencies>
	{
		static rkit::Result AnalyzeFile(AnoxAbstractSingleFileResourceLoaderState &state,
		rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs);
	};

	template<class TLoaderInfo>
	struct SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, true, true>
	{
		static rkit::Result AnalyzeFile(AnoxAbstractSingleFileResourceLoaderState &state,
			rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs);
	};

	template<class TLoaderInfo>
	struct SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, true, false>
	{
		static rkit::Result AnalyzeFile(AnoxAbstractSingleFileResourceLoaderState &state,
			rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs);
	};

	template<class TLoaderInfo, bool THasLoadPhase>
	struct SingleFileResourceLoaderLoadHelper
	{
	};

	template<class TLoaderInfo>
	struct SingleFileResourceLoaderLoadHelper<TLoaderInfo, false>
	{
		static rkit::Result LoadFile(AnoxAbstractSingleFileResourceLoaderState &state);
	};

	template<class TLoaderInfo>
	struct SingleFileResourceLoaderLoadHelper<TLoaderInfo, true>
	{
		static rkit::Result LoadFile(AnoxAbstractSingleFileResourceLoaderState &state);
	};
} }

namespace anox
{
	struct AnoxAbstractSingleFileResourceLoaderState;

	struct AnoxSingleFileResourceLoaderCallbacks
	{
		bool m_hasDependencies;
		bool m_hasAnalysisPhase;
		bool m_hasLoadPhase;
		rkit::Result(*m_analyzeFile)(
			AnoxAbstractSingleFileResourceLoaderState &state,
			rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs);
		rkit::Result(*m_loadFile)(
			AnoxAbstractSingleFileResourceLoaderState &state);
	};

	template<class TLoaderInfo>
	struct AnoxSingleFileResourceLoaderCallbacksFor
	{
		static const AnoxSingleFileResourceLoaderCallbacks ms_callbacks;
	};

	template<class TLoaderInfo>
	const AnoxSingleFileResourceLoaderCallbacks AnoxSingleFileResourceLoaderCallbacksFor<TLoaderInfo>::ms_callbacks =
	{
		TLoaderInfo::kHasDependencies,
		TLoaderInfo::kHasAnalysisPhase,
		TLoaderInfo::kHasLoadPhase,

		priv::SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, TLoaderInfo::kHasAnalysisPhase, TLoaderInfo::kHasDependencies>::AnalyzeFile,
		priv::SingleFileResourceLoaderLoadHelper<TLoaderInfo, TLoaderInfo::kHasLoadPhase>::LoadFile,
	};

	template<class TLoaderInfo>
	class AnoxAbstractSingleFileResourceLoader final : public TLoaderInfo::LoaderBase_t
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<typename TLoaderInfo::LoaderBase_t::ResourceBase_t> &resource, const AnoxResourceLoaderSystems &systems, const typename TLoaderInfo::LoaderBase_t::KeyType_t &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<typename TLoaderInfo::LoaderBase_t::ResourceBase_t> &outResource) const override;
	};

	struct AnoxAbstractSingleFileResourceLoaderState : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_fileContents;
		AnoxResourceBase *m_resource = nullptr;
		AnoxResourceLoaderSystems m_systems = {};
		const AnoxSingleFileResourceLoaderCallbacks *m_functions = nullptr;
	};

	class AnoxAbstractSingleFileLoaderAnalyzeJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxAbstractSingleFileLoaderAnalyzeJob(
			const rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> &state,
			const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> m_state;
		rkit::RCPtr<rkit::JobSignaller> m_waitForDependenciesSignaller;
	};

	class AnoxAbstractSingleFileLoaderLoadJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxAbstractSingleFileLoaderLoadJob(
			const rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> &state);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> m_state;
	};
}

#include "rkit/Core/VectorTrait.h"

namespace anox { namespace priv {

	template<class TLoaderInfo, bool THasDependencies>
	rkit::Result SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, false, THasDependencies>::AnalyzeFile(AnoxAbstractSingleFileResourceLoaderState &state,
		rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs)
	{
		// No analysis phase
		return rkit::ResultCode::kOK;
	}


	template<class TLoaderInfo>
	rkit::Result SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, true, true>::AnalyzeFile(AnoxAbstractSingleFileResourceLoaderState &state,
		rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs)
	{
		using JobRC = typename rkit::RCPtr<rkit::Job>;
		using VTrait = typename rkit::VectorTrait<JobRC>;
		using VTraitRef = typename rkit::traits::TraitRef<VTrait>;
		// Analysis phase with dependencies
		typename TLoaderInfo::State_t &derivedState = static_cast<typename TLoaderInfo::State_t &>(state);
		typename TLoaderInfo::Resource_t &derivedResource = *static_cast<typename TLoaderInfo::Resource_t *>(state.m_resource);
		return TLoaderInfo::AnalyzeFile(derivedState, derivedResource, VTraitRef(outDependencyJobs));
	}

	template<class TLoaderInfo>
	rkit::Result SingleFileResourceLoaderAnalysisHelper<TLoaderInfo, true, false>::AnalyzeFile(AnoxAbstractSingleFileResourceLoaderState &state,
		rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs)
	{
		using JobRC = typename rkit::RCPtr<rkit::Job>;
		using VTrait = typename rkit::VectorTrait<JobRC>;
		// Analysis phase with no dependencies
		typename TLoaderInfo::State_t &derivedState = static_cast<typename TLoaderInfo::State_t &>(state);
		typename TLoaderInfo::Resource_t &derivedResource = *static_cast<typename TLoaderInfo::Resource_t *>(state.m_resource);
		return TLoaderInfo::AnalyzeFile(derivedState, derivedResource);
	}

	template<class TLoaderInfo>
	rkit::Result SingleFileResourceLoaderLoadHelper<TLoaderInfo, false>::LoadFile(AnoxAbstractSingleFileResourceLoaderState &state)
	{
		// No load phase
		typename TLoaderInfo::State_t &derivedState = static_cast<typename TLoaderInfo::State_t &>(state);
		typename TLoaderInfo::Resource_t &derivedResource = *static_cast<typename TLoaderInfo::Resource_t *>(state.m_resource);
		return rkit::ResultCode::kOK;
	}

	template<class TLoaderInfo>
	rkit::Result SingleFileResourceLoaderLoadHelper<TLoaderInfo, true>::LoadFile(AnoxAbstractSingleFileResourceLoaderState &state)
	{
		using JobRC = typename rkit::RCPtr<rkit::Job>;
		using VTrait = typename rkit::VectorTrait<JobRC>;

		// Load phase
		typename TLoaderInfo::State_t &derivedState = static_cast<typename TLoaderInfo::State_t &>(state);
		typename TLoaderInfo::Resource_t &derivedResource = *static_cast<typename TLoaderInfo::Resource_t *>(state.m_resource);
		return TLoaderInfo::LoadFile(derivedState, derivedResource);
	}
} }

namespace anox
{
	template<class TLoaderInfo>
	rkit::Result AnoxAbstractSingleFileResourceLoader<TLoaderInfo>::CreateLoadJob(const rkit::RCPtr<typename TLoaderInfo::LoaderBase_t::ResourceBase_t> &resourceBase, const AnoxResourceLoaderSystems &systems, const typename TLoaderInfo::LoaderBase_t::KeyType_t &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		// Jobs:
		// - Load file job
		// - Analysis job (queues dependency jobs)
		// - Wait for dependencies job (signalled by join job created by analysis job)
		// - Process job
		AnoxGameFileSystemBase &fileSystem = *systems.m_fileSystem;
		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> loaderState;
		RKIT_CHECK(rkit::New<AnoxAbstractSingleFileResourceLoaderState>(loaderState));

		loaderState->m_functions = &AnoxSingleFileResourceLoaderCallbacksFor<TLoaderInfo>::ms_callbacks;
		loaderState->m_resource = resourceBase.Get();
		loaderState->m_systems = systems;

		rkit::RCPtr<rkit::Job> waitForDependenciesJob;
		rkit::RCPtr<rkit::Job> loadJob;

		if (TLoaderInfo::kHasAnalysisPhase)
		{
			rkit::RCPtr<rkit::JobSignaller> waitForDependenciesSignaller;
			if (TLoaderInfo::kHasDependencies)
			{
				RKIT_CHECK(jobQueue.CreateSignalledJob(waitForDependenciesSignaller, waitForDependenciesJob));
			}

			rkit::RCPtr<rkit::Job> loadFileJob;
			RKIT_CHECK(CreateLoadEntireFileJob(loadFileJob, loaderState.FieldRef(&AnoxAbstractSingleFileResourceLoaderState::m_fileContents), fileSystem, key));

			rkit::UniquePtr<rkit::IJobRunner> analysisJobRunner;
			RKIT_CHECK(rkit::New<AnoxAbstractSingleFileLoaderAnalyzeJob>(analysisJobRunner, loaderState, waitForDependenciesSignaller));

			rkit::RCPtr<rkit::Job> analysisJob;
			RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(analysisJobRunner), loadFileJob));

			if (!TLoaderInfo::kHasDependencies)
			{
				waitForDependenciesJob = analysisJob;
			}
		}

		if (TLoaderInfo::kHasLoadPhase)
		{
			rkit::UniquePtr<rkit::IJobRunner> loadJobRunner;
			RKIT_CHECK(rkit::New<AnoxAbstractSingleFileLoaderLoadJob>(loadJobRunner, loaderState));

			RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(loadJobRunner), waitForDependenciesJob));
		}
		else
		{
			loadJob = waitForDependenciesJob;
		}

		outJob = std::move(loadJob);

		return rkit::ResultCode::kOK;
	}

	template<class TLoaderInfo>
	rkit::Result AnoxAbstractSingleFileResourceLoader<TLoaderInfo>::CreateResourceObject(rkit::UniquePtr<typename TLoaderInfo::LoaderBase_t::ResourceBase_t> &outResource) const
	{
		return rkit::New<typename TLoaderInfo::Resource_t>(outResource);
	}
}
