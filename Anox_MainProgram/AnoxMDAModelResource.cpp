#include "AnoxMDAModelResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"

#include "anox/Data/MDAModel.h"

#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

namespace anox
{
	class AnoxMDAModelResource final : public AnoxMDAModelResourceBase
	{
	public:

	};

	class AnoxMDAModelResourceLoader final : public AnoxMDAModelResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxMDAModelResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxMDAModelResourceBase> &outResource) const override;
	};

	struct AnoxMDAModelResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_mdaFileContents;
	};

	class AnoxMDAModelLoaderAnalyzeJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxMDAModelLoaderAnalyzeJob(const rkit::RCPtr<AnoxMDAModelResource> &resource,
			const rkit::RCPtr<AnoxMDAModelResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager,
			const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller);

		rkit::Result Run() override;

	public:
		rkit::RCPtr<AnoxMDAModelResource> m_resource;
		rkit::RCPtr<AnoxMDAModelResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
		rkit::RCPtr<rkit::JobSignaller> m_waitForDependenciesSignaller;
	};

	AnoxMDAModelLoaderAnalyzeJob::AnoxMDAModelLoaderAnalyzeJob(const rkit::RCPtr<AnoxMDAModelResource> &resource,
		const rkit::RCPtr<AnoxMDAModelResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
		AnoxResourceManagerBase &resManager,
		const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller)
		: m_resource(resource)
		, m_state(state)
		, m_jobQueue(jobQueue)
		, m_resManager(resManager)
		, m_waitForDependenciesSignaller(waitForDependenciesSignaller)
	{
	}

	rkit::Result AnoxMDAModelLoaderAnalyzeJob::Run()
	{
		rkit::ReadOnlyMemoryStream stream(m_state->m_mdaFileContents.GetBuffer(), m_state->m_mdaFileContents.Count());


		/*
		const data::UserMDAModel &edef = m_resource->GetMDAModel();

		if (edef.m_modelContentID.IsNull())
		{
			m_waitForDependenciesSignaller->SignalDone(rkit::ResultCode::kOK);
		}
		else
		{
			rkit::RCPtr<rkit::Job> loadModelJob;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(m_resManager.GetContentIDKeyedResource(&loadModelJob, result, resloaders::kMDAModelResourceTypeCode, edef.m_modelContentID));

			rkit::UniquePtr<rkit::IJobRunner> dependenciesDoneJobRunner;
			RKIT_CHECK(m_jobQueue.CreateSignalJobRunner(dependenciesDoneJobRunner, m_waitForDependenciesSignaller));
			RKIT_CHECK(m_jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(dependenciesDoneJobRunner), loadModelJob));
		}
		*/

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMDAModelResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxMDAModelResourceBase> &resourceBase, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		// Jobs:
		// - Load file job
		// - Analysis job (queues dependency jobs)
		// - Wait for dependencies job (signalled by join job created by analysis job)
		// - Process job
		rkit::RCPtr<AnoxMDAModelResource> resource = resourceBase.StaticCast<AnoxMDAModelResource>();

		AnoxGameFileSystemBase &fileSystem = *systems.m_fileSystem;
		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<AnoxMDAModelResourceLoaderState> loaderState;
		RKIT_CHECK(rkit::New<AnoxMDAModelResourceLoaderState>(loaderState));

		rkit::RCPtr<rkit::Job> waitForDependenciesJob;
		rkit::RCPtr<rkit::JobSignaller> waitForDependenciesSignaller;
		RKIT_CHECK(jobQueue.CreateSignalledJob(waitForDependenciesSignaller, waitForDependenciesJob));

		rkit::RCPtr<rkit::Job> loadFileJob;
		RKIT_CHECK(CreateLoadEntireFileJob(loadFileJob, loaderState.FieldRef(&AnoxMDAModelResourceLoaderState::m_mdaFileContents), fileSystem, key));

		rkit::UniquePtr<rkit::IJobRunner> analysisJobRunner;
		RKIT_CHECK(rkit::New<AnoxMDAModelLoaderAnalyzeJob>(analysisJobRunner, resource, loaderState, jobQueue, *systems.m_resManager, waitForDependenciesSignaller));

		rkit::RCPtr<rkit::Job> analysisJob;
		RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(analysisJobRunner), loadFileJob));

		outJob = std::move(waitForDependenciesJob);

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxMDAModelResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxMDAModelResourceBase> &outResource) const
	{
		return rkit::New<AnoxMDAModelResource>(outResource);
	}

	rkit::Result AnoxMDAModelResourceLoaderBase::Create(rkit::RCPtr<AnoxMDAModelResourceLoaderBase> &outLoader)
	{
		rkit::RCPtr<AnoxMDAModelResourceLoader> loader;
		RKIT_CHECK(rkit::New<AnoxMDAModelResourceLoader>(loader));

		outLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
