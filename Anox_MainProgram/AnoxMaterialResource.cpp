#include "AnoxMaterialResource.h"
#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

#include "anox/Data/MaterialData.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class AnoxMaterialResource final : public AnoxMaterialResourceBase
	{
	public:
		explicit AnoxMaterialResource(data::MaterialResourceType materialType);

	private:
		data::MaterialResourceType m_materialType;
	};

	class AnoxMaterialResourceLoader final : public AnoxMaterialResourceLoaderBase
	{
	public:
		explicit AnoxMaterialResourceLoader(data::MaterialResourceType materialType);

		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxMaterialResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxMaterialResourceBase> &outResource) const override;

	private:
		data::MaterialResourceType m_materialType;
	};

	struct AnoxMaterialResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_data;
	};

	class AnoxMaterialAnalysisJobRunner final : public rkit::IJobRunner
	{
	public:
		explicit AnoxMaterialAnalysisJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
			const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager,
			const rkit::RCPtr<rkit::JobSignaler> &waitForDependenciesSignaler);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxMaterialResource> m_resource;
		rkit::RCPtr<AnoxMaterialResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
		rkit::RCPtr<rkit::JobSignaler> m_waitForDependenciesSignaler;
	};

	class AnoxMaterialProcessJobRunner final : public rkit::IJobRunner
	{
	public:
		explicit AnoxMaterialProcessJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
			const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxMaterialResource> m_resource;
		rkit::RCPtr<AnoxMaterialResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
	};

	AnoxMaterialResource::AnoxMaterialResource(data::MaterialResourceType materialType)
		: m_materialType(materialType)
	{
	}

	AnoxMaterialResourceLoader::AnoxMaterialResourceLoader(data::MaterialResourceType materialType)
		: m_materialType(materialType)
	{
	}

	rkit::Result AnoxMaterialResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxMaterialResourceBase> &resourceBase, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::RCPtr<AnoxMaterialResource> resource = resourceBase.StaticCast<AnoxMaterialResource>();

		rkit::IJobQueue &jobQueue = systems.m_fileSystem->GetJobQueue();

		rkit::RCPtr<AnoxMaterialResourceLoaderState> state;
		RKIT_CHECK(rkit::New<AnoxMaterialResourceLoaderState>(state));

		rkit::RCPtr<rkit::Job> loadFileJob;
		RKIT_CHECK(CreateLoadEntireFileJob(loadFileJob, state.FieldRef(&AnoxMaterialResourceLoaderState::m_data), *systems.m_fileSystem, key));

		rkit::RCPtr<rkit::Job> waitForDependenciesJob;
		rkit::RCPtr<rkit::JobSignaler> waitForDependenciesSignaler;
		RKIT_CHECK(jobQueue.CreateSignaledJob(waitForDependenciesSignaler, waitForDependenciesJob));

		rkit::UniquePtr<rkit::IJobRunner> analysisJobRunner;
		RKIT_CHECK(rkit::New<AnoxMaterialAnalysisJobRunner>(analysisJobRunner, resource, state, jobQueue, *systems.m_resManager, waitForDependenciesSignaler));
		RKIT_CHECK(jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(analysisJobRunner), loadFileJob));

		rkit::UniquePtr<rkit::IJobRunner> processJobRunner;
		RKIT_CHECK(rkit::New<AnoxMaterialProcessJobRunner>(analysisJobRunner, resource, state, jobQueue, *systems.m_resManager));
		RKIT_CHECK(jobQueue.CreateJob(&outJob, rkit::JobType::kNormalPriority, std::move(processJobRunner), waitForDependenciesJob));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxMaterialResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxMaterialResourceBase> &outResource) const
	{
		return rkit::New<AnoxMaterialResource>(outResource, m_materialType);
	}

	AnoxMaterialAnalysisJobRunner::AnoxMaterialAnalysisJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
		const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
		AnoxResourceManagerBase &resManager,
		const rkit::RCPtr<rkit::JobSignaler> &waitForDependenciesSignaler)
		: m_resource(resource)
		, m_state(state)
		, m_jobQueue(jobQueue)
		, m_resManager(resManager)
		, m_waitForDependenciesSignaler(waitForDependenciesSignaler)
	{
	}

	rkit::Result AnoxMaterialAnalysisJobRunner::Run()
	{
		rkit::ReadOnlyMemoryStream stream(m_state->m_data.GetBuffer(), m_state->m_data.Count());

		data::MaterialHeader materialHeader = {};
		RKIT_CHECK(stream.ReadAll(&materialHeader, sizeof(materialHeader)));

		if (materialHeader.m_magic.Get() != data::MaterialHeader::kExpectedMagic
			|| materialHeader.m_version.Get() != data::MaterialHeader::kExpectedVersion)
			return rkit::ResultCode::kDataError;

		rkit::Vector<data::MaterialBitmapDef> bitmapDefs;
		rkit::Vector<data::MaterialFrameDef> frameDefs;

		RKIT_CHECK(bitmapDefs.Resize(materialHeader.m_numBitmaps.Get()));
		RKIT_CHECK(frameDefs.Resize(materialHeader.m_numFrames.Get()));

		RKIT_CHECK(stream.ReadAll(bitmapDefs.GetBuffer(), sizeof(data::MaterialBitmapDef) * bitmapDefs.Count()));
		RKIT_CHECK(stream.ReadAll(frameDefs.GetBuffer(), sizeof(data::MaterialFrameDef) * frameDefs.Count()));

		rkit::Vector<rkit::RCPtr<rkit::Job>> bitmapJobs;
		RKIT_CHECK(bitmapJobs.Resize(bitmapDefs.Count()));

		for (size_t bitmapIndex = 0; bitmapIndex < bitmapDefs.Count(); bitmapIndex++)
		{
			rkit::Future<AnoxResourceRetrieveResult> retrieveResult;
			RKIT_CHECK(m_resManager.GetContentIDKeyedResource(&bitmapJobs[bitmapIndex], retrieveResult, resloaders::kTextureResourceTypeCode, bitmapDefs[bitmapIndex].m_contentID));
		}

		rkit::UniquePtr<rkit::IJobRunner> signalJobRunner;
		RKIT_CHECK(m_jobQueue.CreateSignalJobRunner(signalJobRunner, m_waitForDependenciesSignaler));

		RKIT_CHECK(m_jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(signalJobRunner), bitmapJobs.ToSpan()));

		return rkit::ResultCode::kOK;
	}

	AnoxMaterialProcessJobRunner::AnoxMaterialProcessJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
		const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
		AnoxResourceManagerBase &resManager)
		: m_resource(resource)
		, m_state(state)
		, m_jobQueue(jobQueue)
		, m_resManager(resManager)
	{
	}

	rkit::Result AnoxMaterialProcessJobRunner::Run()
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMaterialResourceLoaderBase::Create(rkit::RCPtr<AnoxMaterialResourceLoaderBase> &outLoader, data::MaterialResourceType resType)
	{
		return rkit::New<AnoxMaterialResourceLoader>(outLoader, resType);
	}
}
