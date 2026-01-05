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
		data::BSPDataChunks m_chunks;
	};

	struct AnoxBSPModelLoaderInfo
	{
		typedef AnoxBSPModelResourceLoaderBase LoaderBase_t;
		typedef AnoxBSPModelResource Resource_t;
		typedef AnoxBSPModelResourceLoaderState State_t;

		static constexpr bool kHasDependencies = true;
		static constexpr bool kHasAnalysisPhase = true;
		static constexpr bool kHasLoadPhase = true;

		static rkit::Result AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadFile(State_t &state, Resource_t &resource);

		class ChunkReader
		{
		public:
			explicit ChunkReader(rkit::IReadStream &readStream);

			template<class T>
			rkit::Result VisitMember(rkit::Vector<T> &vector) const;

		private:
			rkit::IReadStream &m_readStream;
		};
	};

	rkit::Result AnoxBSPModelLoaderInfo::AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::ReadOnlyMemoryStream stream(state.m_fileContents.GetBuffer(), state.m_fileContents.Count());

		data::BSPFile bspFile;
		RKIT_CHECK(stream.ReadAll(&bspFile, sizeof(bspFile)));

		if (bspFile.m_fourCC.Get() != data::BSPFile::kFourCC
			|| bspFile.m_version.Get() != data::BSPFile::kVersion)
			return rkit::ResultCode::kDataError;

		RKIT_CHECK(state.m_chunks.VisitAllChunks(ChunkReader(stream)));

		const data::BSPDataChunks &chunks = state.m_chunks;
		anox::AnoxResourceManagerBase &resManager = *state.m_systems.m_resManager;

		// No SafeAdd since we don't really care about overflow here
		RKIT_CHECK(outDeps.Reserve(chunks.m_materials.Count() + chunks.m_lightmaps.Count() + chunks.m_entityDefs.Count()));

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

		for (const rkit::data::ContentID &edefContentID : chunks.m_entityDefs)
		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(resManager.GetContentIDKeyedResource(&job, result, resloaders::kEntityDefTypeCode, edefContentID));

			RKIT_CHECK(outDeps.Append(job));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxBSPModelLoaderInfo::LoadFile(State_t &state, Resource_t &resource)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	AnoxBSPModelLoaderInfo::ChunkReader::ChunkReader(rkit::IReadStream &readStream)
		: m_readStream(readStream)
	{
	}

	template<class T>
	rkit::Result AnoxBSPModelLoaderInfo::ChunkReader::VisitMember(rkit::Vector<T> &vector) const
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

	rkit::Result AnoxBSPModelResourceLoaderBase::Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxBSPModelLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
