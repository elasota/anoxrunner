#include "AnoxBSPModelResource.h"
#include "AnoxMaterialResource.h"
#include "AnoxLoadEntireFileJob.h"

#include "AnoxGameFileSystem.h"

#include "anox/Data/BSPModel.h"

#include "rkit/Data/ContentID.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class AnoxBSPModelResource final : public AnoxBSPModelResourceBase
	{
	};

	class AnoxBSPModelResourceLoader final : public AnoxBSPModelResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxBSPModelResourceBase> &outResource) const override;
	};

	struct AnoxBSPModelResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_bspFileContents;
		data::BSPDataChunks m_chunks;
	};

	class AnoxBSPLoaderAnalyzeJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxBSPLoaderAnalyzeJob(const rkit::RCPtr<AnoxBSPModelResource> &resource,
			const rkit::RCPtr<AnoxBSPModelResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager,
			const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller);

		rkit::Result Run() override;

	private:
		class ChunkReader
		{
		public:
			explicit ChunkReader(rkit::IReadStream &readStream);

			template<class T>
			rkit::Result VisitMember(rkit::Vector<T> &vector) const;

		private:
			rkit::IReadStream &m_readStream;
		};

		rkit::RCPtr<AnoxBSPModelResource> m_resource;
		rkit::RCPtr<AnoxBSPModelResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
		rkit::RCPtr<rkit::JobSignaller> m_waitForDependenciesSignaller;
	};

	class AnoxBSPLoaderProcessJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxBSPLoaderProcessJob(const rkit::RCPtr<AnoxBSPModelResource> &resource,
			const rkit::RCPtr<AnoxBSPModelResourceLoaderState> &state);

		rkit::Result Run() override;

	private:
	};

	rkit::Result AnoxBSPModelResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resourceBase, const AnoxResourceLoaderSystems &systems, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		// Jobs:
		// - Load file job
		// - Analysis job (queues dependency jobs)
		// - Wait for dependencies job (signalled by join job created by analysis job)
		// - Process job
		rkit::RCPtr<AnoxBSPModelResource> resource = resourceBase.StaticCast<AnoxBSPModelResource>();

		AnoxGameFileSystemBase &fileSystem = *systems.m_fileSystem;
		rkit::IJobQueue &jobQueue = fileSystem.GetJobQueue();

		rkit::RCPtr<AnoxBSPModelResourceLoaderState> loaderState;
		RKIT_CHECK(rkit::New<AnoxBSPModelResourceLoaderState>(loaderState));

		rkit::RCPtr<rkit::Job> waitForDependenciesJob;
		rkit::RCPtr<rkit::JobSignaller> waitForDependenciesSignaller;
		RKIT_CHECK(jobQueue.CreateSignalledJob(waitForDependenciesSignaller, waitForDependenciesJob));

		rkit::RCPtr<rkit::Job> loadFileJob;
		RKIT_CHECK(CreateLoadEntireFileJob(loadFileJob, loaderState.FieldRef(&AnoxBSPModelResourceLoaderState::m_bspFileContents), fileSystem, key));

		rkit::UniquePtr<rkit::IJobRunner> analysisJobRunner;
		RKIT_CHECK(rkit::New<AnoxBSPLoaderAnalyzeJob>(analysisJobRunner, resource, loaderState, jobQueue, *systems.m_resManager, waitForDependenciesSignaller));

		rkit::RCPtr<rkit::Job> analysisJob;
		RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(analysisJobRunner), loadFileJob));

		outJob = std::move(waitForDependenciesJob);

		return rkit::ResultCode::kOK;
	}

	AnoxBSPLoaderAnalyzeJob::AnoxBSPLoaderAnalyzeJob(const rkit::RCPtr<AnoxBSPModelResource> &resource,
		const rkit::RCPtr<AnoxBSPModelResourceLoaderState> &state, rkit::IJobQueue &jobQueue, AnoxResourceManagerBase &resManager, const rkit::RCPtr<rkit::JobSignaller> &waitForDependenciesSignaller)
		: m_resource(resource)
		, m_state(state)
		, m_jobQueue(jobQueue)
		, m_resManager(resManager)
		, m_waitForDependenciesSignaller(waitForDependenciesSignaller)
	{
	}

	rkit::Result AnoxBSPLoaderAnalyzeJob::Run()
	{
		rkit::ReadOnlyMemoryStream stream(m_state->m_bspFileContents.GetBuffer(), m_state->m_bspFileContents.Count());

		data::BSPFile bspFile;
		RKIT_CHECK(stream.ReadAll(&bspFile, sizeof(bspFile)));

		if (bspFile.m_fourCC.Get() != data::BSPFile::kFourCC
			|| bspFile.m_version.Get() != data::BSPFile::kVersion)
			return rkit::ResultCode::kDataError;

		RKIT_CHECK(m_state->m_chunks.VisitAllChunks(ChunkReader(stream)));

		const data::BSPDataChunks &chunks = m_state->m_chunks;

		size_t numDepJobs = 0;

		rkit::Vector<rkit::RCPtr<rkit::Job>> resourceJobs;

		for (const rkit::data::ContentID &materialContentID : chunks.m_materials)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(m_resManager.GetContentIDKeyedResource(&job, result, resloaders::kWorldMaterialTypeCode, materialContentID));

			RKIT_CHECK(resourceJobs.Append(job));
		}

		for (const rkit::data::ContentID &lightmapContentID : chunks.m_lightmaps)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(m_resManager.GetContentIDKeyedResource(&job, result, resloaders::kTextureResourceTypeCode, lightmapContentID));

			RKIT_CHECK(resourceJobs.Append(job));
		}

		{
			rkit::UniquePtr<rkit::IJobRunner> dependenciesDoneJobRunner;
			RKIT_CHECK(m_jobQueue.CreateSignalJobRunner(dependenciesDoneJobRunner, m_waitForDependenciesSignaller));
			RKIT_CHECK(m_jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(dependenciesDoneJobRunner), resourceJobs.ToSpan().ToValueISpan()));
		}

		return rkit::ResultCode::kOK;
	}

	AnoxBSPLoaderAnalyzeJob::ChunkReader::ChunkReader(rkit::IReadStream &readStream)
		: m_readStream(readStream)
	{
	}

	template<class T>
	rkit::Result AnoxBSPLoaderAnalyzeJob::ChunkReader::VisitMember(rkit::Vector<T> &vector) const
	{
		rkit::endian::LittleUInt32_t countData;
		RKIT_CHECK(m_readStream.ReadAll(&countData, sizeof(countData)));

		RKIT_CHECK(vector.Resize(countData.Get()));

		if (vector.Count() > 0)
		{
			RKIT_CHECK(m_readStream.ReadAll(vector.GetBuffer(), vector.Count() * sizeof(T)));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxBSPModelResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxBSPModelResourceBase> &outResource) const
	{
		return rkit::New<AnoxBSPModelResource>(outResource);
	}

	rkit::Result AnoxBSPModelResourceLoaderBase::Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &outFactory)
	{
		rkit::RCPtr<AnoxBSPModelResourceLoader> factory;
		RKIT_CHECK(rkit::New<AnoxBSPModelResourceLoader>(factory));

		outFactory = std::move(factory);

		return rkit::ResultCode::kOK;
	}
}
