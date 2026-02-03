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
	template<class TLoaderInfo>
	struct SingleFileResourceLoaderPhaseHelper
	{
		static rkit::Result LoadPhase(AnoxAbstractSingleFileResourceLoaderState &state,
			size_t phase,
			rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs);
	};
} }

namespace anox
{
	struct AnoxAbstractSingleFileResourceLoaderState;

	struct AnoxSingleFileResourceLoaderCallbacks
	{
		size_t m_numLoadPhases;

		rkit::Result(*m_loadPhaseCallback)(
			AnoxAbstractSingleFileResourceLoaderState &state,
			size_t phase,
			rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs);
	};

	template<class TLoaderInfo>
	struct AnoxSingleFileResourceLoaderCallbacksFor
	{
		static const AnoxSingleFileResourceLoaderCallbacks ms_callbacks;
	};

	template<class TLoaderInfo>
	const AnoxSingleFileResourceLoaderCallbacks AnoxSingleFileResourceLoaderCallbacksFor<TLoaderInfo>::ms_callbacks =
	{
		TLoaderInfo::kNumPhases,

		priv::SingleFileResourceLoaderPhaseHelper<TLoaderInfo>::LoadPhase,
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
		rkit::RCPtr<AnoxResourceBase> m_resource;
		AnoxResourceLoaderSystems m_systems = {};
		const AnoxSingleFileResourceLoaderCallbacks *m_functions = nullptr;
	};

	class AnoxAbstractSingleFileLoaderPhaseJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxAbstractSingleFileLoaderPhaseJob(
			const rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> &state,
			size_t phase,
			const rkit::RCPtr<rkit::JobSignaler> &waitForDependenciesSignaler);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> m_state;
		rkit::RCPtr<rkit::JobSignaler> m_waitForDependenciesSignaler;
		size_t m_phase;
	};
}

#include "rkit/Core/VectorTrait.h"

namespace anox { namespace priv {

	template<class TLoaderInfo>
	rkit::Result SingleFileResourceLoaderPhaseHelper<TLoaderInfo>::LoadPhase(AnoxAbstractSingleFileResourceLoaderState &state,
		size_t phase, rkit::HybridVector<rkit::RCPtr<rkit::Job>, 16> &outDependencyJobs)
	{
		using JobRC = typename rkit::RCPtr<rkit::Job>;
		using VTrait = typename rkit::VectorTrait<JobRC>;
		using VTraitRef = typename rkit::traits::TraitRef<VTrait>;
		// Analysis phase with dependencies
		typename TLoaderInfo::State_t &derivedState = static_cast<typename TLoaderInfo::State_t &>(state);
		typename TLoaderInfo::Resource_t &derivedResource = *static_cast<typename TLoaderInfo::Resource_t *>(state.m_resource.Get());
		return TLoaderInfo::LoadPhase(derivedState, derivedResource, phase, VTraitRef(outDependencyJobs));
	}
} }

namespace anox
{
	template<class TLoaderInfo>
	rkit::Result AnoxAbstractSingleFileResourceLoader<TLoaderInfo>::CreateLoadJob(const rkit::RCPtr<typename TLoaderInfo::LoaderBase_t::ResourceBase_t> &resourceBase, const AnoxResourceLoaderSystems &systems, const typename TLoaderInfo::LoaderBase_t::KeyType_t &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		typedef typename TLoaderInfo::State_t State_t;
		// Jobs:
		// - Load file job
		// - Analysis job (queues dependency jobs)
		// - Wait for dependencies job (signalled by join job created by analysis job)
		// - Process job
		AnoxGameFileSystemBase &fileSystem = *systems.m_fileSystem;
		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<AnoxAbstractSingleFileResourceLoaderState> loaderState;
		RKIT_CHECK(rkit::New<State_t>(loaderState));

		loaderState->m_functions = &AnoxSingleFileResourceLoaderCallbacksFor<TLoaderInfo>::ms_callbacks;
		loaderState->m_resource = resourceBase;
		loaderState->m_systems = systems;

		rkit::RCPtr<rkit::Job> prevPhaseEndJob;
		RKIT_CHECK(CreateLoadEntireFileJob(prevPhaseEndJob, loaderState.FieldRef(&AnoxAbstractSingleFileResourceLoaderState::m_fileContents), fileSystem, key));

		for (size_t phase = 0; phase < TLoaderInfo::kNumPhases; phase++)
		{
			rkit::RCPtr<rkit::Job> waitForDependenciesJob;
			rkit::RCPtr<rkit::Job> loadJob;

			rkit::RCPtr<rkit::JobSignaler> waitForDependenciesSignaler;
			if (TLoaderInfo::PhaseHasDependencies(phase))
			{
				RKIT_CHECK(jobQueue.CreateSignaledJob(waitForDependenciesSignaler, waitForDependenciesJob));
			}

			rkit::UniquePtr<rkit::IJobRunner> phaseJobRunner;
			RKIT_CHECK(rkit::New<AnoxAbstractSingleFileLoaderPhaseJob>(phaseJobRunner, loaderState, phase, waitForDependenciesSignaler));

			rkit::RCPtr<rkit::Job> phaseJob;
			RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(&phaseJob, rkit::JobType::kNormalPriority, std::move(phaseJobRunner), prevPhaseEndJob));

			if (waitForDependenciesSignaler.IsValid())
			{
				prevPhaseEndJob = waitForDependenciesJob;
			}
			else
			{
				prevPhaseEndJob = phaseJob;
			}
		}

		outJob = std::move(prevPhaseEndJob);

		RKIT_RETURN_OK;
	}

	template<class TLoaderInfo>
	rkit::Result AnoxAbstractSingleFileResourceLoader<TLoaderInfo>::CreateResourceObject(rkit::UniquePtr<typename TLoaderInfo::LoaderBase_t::ResourceBase_t> &outResource) const
	{
		return rkit::New<typename TLoaderInfo::Resource_t>(outResource);
	}
}
