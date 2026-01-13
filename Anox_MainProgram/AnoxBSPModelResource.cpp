#include "AnoxBSPModelResource.h"
#include "AnoxMaterialResource.h"
#include "AnoxAbstractSingleFileResource.h"
#include "AnoxGameFileSystem.h"

#include "anox/Data/BSPModel.h"

#include "anox/Data/EntityStructs.h"

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

	struct AnoxBSPModelResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		data::BSPDataChunksSpans m_chunks;
	};

	struct AnoxBSPModelLoaderInfo
	{
		typedef AnoxBSPModelResourceLoaderBase LoaderBase_t;
		typedef AnoxBSPModelResource Resource_t;
		typedef AnoxBSPModelResourceLoaderState State_t;

		static constexpr size_t kNumPhases = 2;

		static rkit::Result LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadContents(State_t &state, Resource_t &resource);

		static bool PhaseHasDependencies(size_t phase);
		static rkit::Result LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);

		class ChunkReader
		{
		public:
			explicit ChunkReader(rkit::FixedSizeMemoryStream &readStream);

			template<class T>
			rkit::Result VisitMember(rkit::Span<T> &span) const;

		private:
			rkit::FixedSizeMemoryStream &m_readStream;
		};
	};

	rkit::Result AnoxBSPModelLoaderInfo::LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		{
			rkit::FixedSizeMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());

			data::BSPFile bspFile;
			RKIT_CHECK(stream.ReadAll(&bspFile, sizeof(bspFile)));

			if (bspFile.m_fourCC.Get() != data::BSPFile::kFourCC
				|| bspFile.m_version.Get() != data::BSPFile::kVersion)
				return rkit::ResultCode::kDataError;

			RKIT_CHECK(data::BSPDataChunksProcessor::VisitAllChunks(state.m_chunks, ChunkReader(stream)));
		}

		const data::BSPDataChunksSpans &chunks = state.m_chunks;
		anox::AnoxResourceManagerBase &resManager = *state.m_systems.m_resManager;

		// No SafeAdd since we don't really care about overflow here
		RKIT_CHECK(outDeps.Reserve(chunks.m_materials.Count() + chunks.m_lightmaps.Count()));

		for (const rkit::data::ContentID &materialContentID : chunks.m_materials)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(resManager.GetContentIDKeyedResource(&job, result, resloaders::kWorldMaterialTypeCode, materialContentID));

			RKIT_CHECK(outDeps.Append(job));
		}

		for (const rkit::data::ContentID &lightmapContentID : chunks.m_lightmaps)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(resManager.GetContentIDKeyedResource(&job, result, resloaders::kTextureResourceTypeCode, lightmapContentID));

			RKIT_CHECK(outDeps.Append(job));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxBSPModelLoaderInfo::LoadContents(State_t &state, Resource_t &resource)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	bool AnoxBSPModelLoaderInfo::PhaseHasDependencies(size_t phase)
	{
		switch (phase)
		{
		case 0:
			return true;
		default:
			return false;
		}
	}

	rkit::Result AnoxBSPModelLoaderInfo::LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		switch (phase)
		{
		case 0:
			return LoadHeaderAndQueueDependencies(state, resource, outDeps);
		case 1:
			return LoadContents(state, resource);
		default:
			return rkit::ResultCode::kInternalError;
		}
	}

	AnoxBSPModelLoaderInfo::ChunkReader::ChunkReader(rkit::FixedSizeMemoryStream &readStream)
		: m_readStream(readStream)
	{
	}

	template<class T>
	rkit::Result AnoxBSPModelLoaderInfo::ChunkReader::VisitMember(rkit::Span<T> &span) const
	{
		rkit::endian::LittleUInt32_t countData;
		RKIT_CHECK(m_readStream.ReadOneBinary(countData));

		uint32_t count = countData.Get();

		if (count == 0)
		{
			span = rkit::Span<T>();
		}
		else
		{
			RKIT_CHECK(m_readStream.ExtractSpan(span, count));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxBSPModelResourceLoaderBase::Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxBSPModelLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
