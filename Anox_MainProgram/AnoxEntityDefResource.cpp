#include "AnoxEntityDefResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"

#include "anox/Data/EntityDef.h"

#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

namespace anox
{
	class AnoxEntityDefResource final : public AnoxEntityDefResourceBase
	{
	public:
		const data::UserEntityDef &GetEntityDef() const override;

		data::UserEntityDef &ModifyUserEntityDef();

	private:
		data::UserEntityDef m_userEntityDef;
	};

	class AnoxEntityDefResourceLoader final : public AnoxEntityDefResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxEntityDefResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxEntityDefResourceBase> &outResource) const override;
	};

	struct AnoxEntityDefResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_edefFileContents;
	};

	class AnoxEntityDefLoaderAnalyzeJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxEntityDefLoaderAnalyzeJob(const rkit::RCPtr<AnoxEntityDefResource> &resource,
			const rkit::RCPtr<AnoxEntityDefResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager,
			const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller);

		rkit::Result Run() override;

	public:
		rkit::RCPtr<AnoxEntityDefResource> m_resource;
		rkit::RCPtr<AnoxEntityDefResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
		rkit::RCPtr<rkit::JobSignaller> m_waitForDependenciesSignaller;
	};

	const data::UserEntityDef &AnoxEntityDefResource::GetEntityDef() const
	{
		return m_userEntityDef;
	}

	data::UserEntityDef &AnoxEntityDefResource::ModifyUserEntityDef()
	{
		return m_userEntityDef;
	}

	AnoxEntityDefLoaderAnalyzeJob::AnoxEntityDefLoaderAnalyzeJob(const rkit::RCPtr<AnoxEntityDefResource> &resource,
		const rkit::RCPtr<AnoxEntityDefResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
		AnoxResourceManagerBase &resManager,
		const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller)
		: m_resource(resource)
		, m_state(state)
		, m_jobQueue(jobQueue)
		, m_resManager(resManager)
		, m_waitForDependenciesSignaller(waitForDependenciesSignaller)
	{
	}

	rkit::Result AnoxEntityDefLoaderAnalyzeJob::Run()
	{
		rkit::ReadOnlyMemoryStream stream(m_state->m_edefFileContents.GetBuffer(), m_state->m_edefFileContents.Count());

		RKIT_CHECK(stream.ReadOneBinary(m_resource->ModifyUserEntityDef()));

		const data::UserEntityDef &edef = m_resource->GetEntityDef();

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

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxEntityDefResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxEntityDefResourceBase> &resourceBase, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		// Jobs:
		// - Load file job
		// - Analysis job (queues dependency jobs)
		// - Wait for dependencies job (signalled by join job created by analysis job)
		// - Process job
		rkit::RCPtr<AnoxEntityDefResource> resource = resourceBase.StaticCast<AnoxEntityDefResource>();

		AnoxGameFileSystemBase &fileSystem = *systems.m_fileSystem;
		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<AnoxEntityDefResourceLoaderState> loaderState;
		RKIT_CHECK(rkit::New<AnoxEntityDefResourceLoaderState>(loaderState));

		rkit::RCPtr<rkit::Job> waitForDependenciesJob;
		rkit::RCPtr<rkit::JobSignaller> waitForDependenciesSignaller;
		RKIT_CHECK(jobQueue.CreateSignalledJob(waitForDependenciesSignaller, waitForDependenciesJob));

		rkit::RCPtr<rkit::Job> loadFileJob;
		RKIT_CHECK(CreateLoadEntireFileJob(loadFileJob, loaderState.FieldRef(&AnoxEntityDefResourceLoaderState::m_edefFileContents), fileSystem, key));

		rkit::UniquePtr<rkit::IJobRunner> analysisJobRunner;
		RKIT_CHECK(rkit::New<AnoxEntityDefLoaderAnalyzeJob>(analysisJobRunner, resource, loaderState, jobQueue, *systems.m_resManager, waitForDependenciesSignaller));

		rkit::RCPtr<rkit::Job> analysisJob;
		RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(analysisJobRunner), loadFileJob));

		outJob = std::move(waitForDependenciesJob);

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxEntityDefResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxEntityDefResourceBase> &outResource) const
	{
		return rkit::New<AnoxEntityDefResource>(outResource);
	}


	rkit::Result AnoxEntityDefResourceLoaderBase::Create(rkit::RCPtr<AnoxEntityDefResourceLoaderBase> &outLoader)
	{
		rkit::RCPtr<AnoxEntityDefResourceLoader> loader;
		RKIT_CHECK(rkit::New<AnoxEntityDefResourceLoader>(loader));

		outLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
