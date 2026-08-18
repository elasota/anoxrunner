#include "AnoxMaterialResource.h"
#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

#include "AnoxTextureResource.h"

#include "anox/Data/MaterialData.h"
#include "anox/Data/ResourceTypeCodes.h"

#include "rkit/Core/DebugString.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class AnoxMaterialResourceLoader;
	class AnoxMaterialAnalysisJobRunner;

	class AnoxMaterialResourceImpl final : public rkit::OpaqueImplementation<AnoxMaterialResource>
	{
	public:
		friend class AnoxMaterialResourceLoader;

	private:
		data::MaterialResourceType m_materialType = data::MaterialResourceType::kCount;
		rkit::Vector<AnoxMaterialResource::FrameDef> m_frames;
		rkit::Vector<rkit::RCPtr<AnoxTextureResourceBase>> m_bitmaps;
		AnoxMaterialResource::InterformData m_interformData;
	};

	class AnoxMaterialResourceLoader final : public AnoxMaterialResourceLoaderBase
	{
	public:
		explicit AnoxMaterialResourceLoader(data::MaterialResourceType materialType);

		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxMaterialResource> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxMaterialResource> &outResource) const override;

	private:
		data::MaterialResourceType m_materialType;
	};

	struct AnoxMaterialResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_data;

		rkit::Vector<rkit::Future<AnoxResourceRetrieveResult>> m_bitmapRetrieveResults;
	};

	class AnoxMaterialAnalysisJobRunner final : public rkit::IJobRunner
	{
	public:
		explicit AnoxMaterialAnalysisJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
			rkit::Vector<AnoxMaterialResource::FrameDef> &outFrameDefs,
			AnoxMaterialResource::InterformData &outInterformData,
			const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager,
			const rkit::RCPtr<rkit::JobSignaler> &waitForDependenciesSignaler);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxMaterialResource> m_resource;
		rkit::Vector<AnoxMaterialResource::FrameDef> &m_outFrameDefs;
		AnoxMaterialResource::InterformData &m_outInterformData;
		rkit::RCPtr<AnoxMaterialResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
		rkit::RCPtr<rkit::JobSignaler> m_waitForDependenciesSignaler;
	};

	class AnoxMaterialProcessJobRunner final : public rkit::IJobRunner
	{
	public:
		explicit AnoxMaterialProcessJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
			rkit::Vector<rkit::RCPtr<AnoxTextureResourceBase>> &outTextures,
			const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
			AnoxResourceManagerBase &resManager);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxMaterialResource> m_resource;
		rkit::Vector<rkit::RCPtr<AnoxTextureResourceBase>> &m_outTextures;
		rkit::RCPtr<AnoxMaterialResourceLoaderState> m_state;
		rkit::IJobQueue &m_jobQueue;
		AnoxResourceManagerBase &m_resManager;
	};

	AnoxMaterialResourceLoader::AnoxMaterialResourceLoader(data::MaterialResourceType materialType)
		: m_materialType(materialType)
	{
	}

	rkit::Result AnoxMaterialResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxMaterialResource> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::IJobQueue &jobQueue = systems.m_fileSystem->GetJobQueue();

		rkit::RCPtr<AnoxMaterialResourceLoaderState> state = rkit::NewRC<AnoxMaterialResourceLoaderState>();

		rkit::RCPtr<rkit::Job> loadFileJob;
		CreateLoadEntireFileJob(loadFileJob, state.FieldRef(&AnoxMaterialResourceLoaderState::m_data), *systems.m_fileSystem, key);

		rkit::RCPtr<rkit::Job> waitForDependenciesJob;
		rkit::RCPtr<rkit::JobSignaler> waitForDependenciesSignaler;
		jobQueue.CreateSignaledJob(waitForDependenciesSignaler, waitForDependenciesJob);

		rkit::UniquePtr<rkit::IJobRunner> analysisJobRunner = rkit::New<AnoxMaterialAnalysisJobRunner>(
			resource,
			resource->Impl().m_frames,
			resource->Impl().m_interformData,
			state,
			jobQueue,
			*systems.m_resManager,
			waitForDependenciesSignaler);
		jobQueue.CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(analysisJobRunner), loadFileJob);

		rkit::UniquePtr<rkit::IJobRunner> processJobRunner = rkit::New<AnoxMaterialProcessJobRunner>(
			resource,
			resource->Impl().m_bitmaps,
			state,
			jobQueue,
			*systems.m_resManager);
		jobQueue.CreateJob(&outJob, rkit::JobType::kNormalPriority, std::move(processJobRunner), waitForDependenciesJob);

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxMaterialResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxMaterialResource> &outResource) const
	{
		rkit::UniquePtr<AnoxMaterialResource> resource = rkit::New<AnoxMaterialResource>();

		resource->Impl().m_materialType = m_materialType;

		outResource = std::move(resource);
	}

	AnoxMaterialAnalysisJobRunner::AnoxMaterialAnalysisJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
		rkit::Vector<AnoxMaterialResource::FrameDef> &outFrameDefs,
		AnoxMaterialResource::InterformData &outInterformData,
		const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
		AnoxResourceManagerBase &resManager,
		const rkit::RCPtr<rkit::JobSignaler> &waitForDependenciesSignaler)
		: m_resource(resource)
		, m_outFrameDefs(outFrameDefs)
		, m_outInterformData(outInterformData)
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
		stream.ReadAll(&materialHeader, sizeof(materialHeader));

		if (materialHeader.m_magic.Get() != data::MaterialHeader::kExpectedMagic
			|| materialHeader.m_version.Get() != data::MaterialHeader::kExpectedVersion)
			RKIT_THROW(rkit::ResultCode::kDataError);

		rkit::Vector<data::MaterialBitmapDef> bitmapDefs;
		rkit::Vector<data::MaterialFrameDef> frameDefs;
		rkit::Optional<data::MaterialInterformData> interformDefOpt;

		if (materialHeader.m_materialType >= static_cast<size_t>(data::MaterialType::kCount)
			|| materialHeader.m_colorType >= static_cast<size_t>(data::MaterialColorType::kCount))
			RKIT_THROW(rkit::ResultCode::kDataError);

		bitmapDefs.Resize(materialHeader.m_numBitmaps.Get());
		frameDefs.Resize(materialHeader.m_numFrames.Get());

		if (bitmapDefs.Count() == 0 || frameDefs.Count() == 0)
			RKIT_THROW(rkit::ResultCode::kDataError);

		stream.ReadAllSpan(bitmapDefs.ToSpan());
		stream.ReadAllSpan(frameDefs.ToSpan());

		if (static_cast<data::MaterialType>(materialHeader.m_materialType) == data::MaterialType::kInterform)
		{
			data::MaterialInterformData interformData;
			stream.ReadOneBinary(interformData);

			interformDefOpt = interformData;
		}

		// Convert frame defs
		{
			m_outFrameDefs.Resize(frameDefs.Count());

			const size_t numBitmaps = bitmapDefs.Count();
			const size_t numFrames = frameDefs.Count();
			rkit::ProcessParallelSpans(m_outFrameDefs.ToSpan(), frameDefs.ToSpan(), [numBitmaps, numFrames](AnoxMaterialResource::FrameDef &outFrameDef, const data::MaterialFrameDef &inFrameDef)
				{
					outFrameDef.m_bitmapIndex = inFrameDef.m_bitmap.Get();
					outFrameDef.m_next = inFrameDef.m_next.Get();
					outFrameDef.m_waitMSec = inFrameDef.m_waitMSec.Get();

					if (outFrameDef.m_bitmapIndex >= numBitmaps || outFrameDef.m_next >= numFrames)
						RKIT_THROW(rkit::ResultCode::kDataError);
				});
		}

		// Convert interform
		if (interformDefOpt.IsSet())
		{
			const size_t numBitmaps = bitmapDefs.Count();

			const data::MaterialInterformData& inInterformData = interformDefOpt.Get();
			AnoxMaterialResource::InterformData &outInterformData = m_outInterformData;

			outInterformData.m_paletteBitmap = inInterformData.m_paletteBitmap.Get();

			rkit::ProcessParallelSpans(rkit::Span<AnoxMaterialResource::InterformHalf>(outInterformData.m_halves),
				rkit::Span<const data::MaterialInterformHalf>(inInterformData.m_halves),
				[numBitmaps](AnoxMaterialResource::InterformHalf &outHalf, const data::MaterialInterformHalf &inHalf)
				{
					outHalf.m_movement = inHalf.m_movement;
					outHalf.m_bitmap = inHalf.m_bitmap.Get();
					outHalf.m_vx = inHalf.m_vx.Get();
					outHalf.m_vy = inHalf.m_vy.Get();
					outHalf.m_rate = inHalf.m_rate.Get();
					outHalf.m_strength = inHalf.m_strength.Get();
					outHalf.m_speed = inHalf.m_speed.Get();

					if (outHalf.m_bitmap >= numBitmaps)
						RKIT_THROW(rkit::ResultCode::kDataError);
				});

			if (outInterformData.m_paletteBitmap >= numBitmaps)
				RKIT_THROW(rkit::ResultCode::kDataError);
		}


		rkit::Vector<rkit::RCPtr<rkit::Job>> bitmapJobs;
		bitmapJobs.Resize(bitmapDefs.Count());

		m_state->m_bitmapRetrieveResults.Resize(bitmapDefs.Count());

		for (size_t bitmapIndex = 0; bitmapIndex < bitmapDefs.Count(); bitmapIndex++)
			m_resManager.GetContentIDKeyedResource(&bitmapJobs[bitmapIndex], m_state->m_bitmapRetrieveResults[bitmapIndex], resloaders::kTextureResourceTypeCode, bitmapDefs[bitmapIndex].m_contentID);

		rkit::UniquePtr<rkit::IJobRunner> signalJobRunner;
		m_jobQueue.CreateSignalJobRunner(signalJobRunner, m_waitForDependenciesSignaler);

		rkit::RCPtr<rkit::Job> job;
		m_jobQueue.CreateJob(&job, rkit::JobType::kNormalPriority, std::move(signalJobRunner), bitmapJobs.ToSpan());

		m_state->m_data.Reset();
	}

	AnoxMaterialProcessJobRunner::AnoxMaterialProcessJobRunner(const rkit::RCPtr<AnoxMaterialResource> &resource,
		rkit::Vector<rkit::RCPtr<AnoxTextureResourceBase>> &outTextures,
		const rkit::RCPtr<AnoxMaterialResourceLoaderState> &state, rkit::IJobQueue &jobQueue,
		AnoxResourceManagerBase &resManager)
		: m_resource(resource)
		, m_outTextures(outTextures)
		, m_state(state)
		, m_jobQueue(jobQueue)
		, m_resManager(resManager)
	{
	}

	rkit::Result AnoxMaterialProcessJobRunner::Run()
	{
		m_outTextures.Resize(m_state->m_bitmapRetrieveResults.Count());

		rkit::ProcessParallelSpans(m_outTextures.ToSpan(), m_state->m_bitmapRetrieveResults.ToSpan(), [](rkit::RCPtr<AnoxTextureResourceBase> &outTexture, const rkit::Future<AnoxResourceRetrieveResult> &inResult)
			{
				if (AnoxResourceRetrieveResult* result = inResult.TryGetResult())
					outTexture = result->m_resourceHandle.StaticCast<AnoxTextureResourceBase>();
			});
	}

	rkit::Result AnoxMaterialResourceLoaderBase::Create(rkit::RCPtr<AnoxMaterialResourceLoaderBase> &outLoader, data::MaterialResourceType resType)
	{
		outLoader = rkit::NewRC<AnoxMaterialResourceLoader>(resType);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::AnoxMaterialResourceImpl)
