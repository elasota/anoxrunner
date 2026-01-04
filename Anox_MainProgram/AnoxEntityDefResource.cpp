#include "AnoxEntityDefResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/VectorTrait.h"

#include "anox/Data/EntityDef.h"

#include "AnoxDataReader.h"
#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

#include "AnoxAbstractSingleFileResource.h"

namespace anox
{
	class AnoxEntityDefResource;
	struct AnoxEntityDefResourceLoaderState;

	struct AnoxEntityDefResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		data::UserEntityDef m_edef;
	};

	struct AnoxEntityDefLoaderInfo
	{
		typedef AnoxEntityDefResourceLoaderBase LoaderBase_t;
		typedef AnoxEntityDefResource Resource_t;
		typedef AnoxEntityDefResourceLoaderState State_t;

		static constexpr bool kHasDependencies = true;
		static constexpr bool kHasAnalysisPhase = true;
		static constexpr bool kHasLoadPhase = true;

		static rkit::Result AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadFile(State_t &state, Resource_t &resource);
	};

	class AnoxEntityDefResource final : public AnoxEntityDefResourceBase
	{
	public:
		friend struct AnoxEntityDefLoaderInfo;

		const Values &GetValues() const override;

	private:
		Values m_values;
	};

	rkit::Result AnoxEntityDefLoaderInfo::AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::ReadOnlyMemoryStream stream(state.m_fileContents.ToSpan());

		data::UserEntityDef &edef = state.m_edef;

		RKIT_CHECK(stream.ReadOneBinary(edef));

		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(state.m_systems.m_resManager->GetContentIDKeyedResource(&job, result, resloaders::kMDAModelResourceTypeCode, edef.m_modelContentID));

			RKIT_CHECK(outDeps.AppendRValue(std::move(job)));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxEntityDefLoaderInfo::LoadFile(State_t &state, Resource_t &resource)
	{
		AnoxEntityDefResource::Values &resValues = resource.m_values;

		resource.m_values.m_modelCodeFourCC = state.m_edef.m_modelCode.Get();

		RKIT_CHECK(DataReader::ReadCheckVec(resource.m_values.m_scale, state.m_edef.m_scale, 16));

		rkit::endian::LittleFloat32_t m_scale[3];
		rkit::endian::LittleUInt32_t m_entityType;
		uint8_t m_shadowType = 0;
		rkit::endian::LittleFloat32_t m_bboxMin[3];
		rkit::endian::LittleFloat32_t m_bboxMax[3];
		uint8_t m_flags = 0;
		rkit::endian::LittleFloat32_t m_walkSpeed;
		rkit::endian::LittleFloat32_t m_runSpeed;
		rkit::endian::LittleFloat32_t m_speed;
		rkit::endian::LittleUInt32_t m_targetSequenceID;
		rkit::endian::LittleUInt32_t m_miscValue;
		rkit::endian::LittleUInt32_t m_startSequenceID;
		uint8_t m_descriptionStringLength = 0;

		return rkit::ResultCode::kNotYetImplemented;
	}

	const AnoxEntityDefResource::Values &AnoxEntityDefResource::GetValues() const
	{
		return m_values;
	}

	rkit::Result AnoxEntityDefResourceLoaderBase::Create(rkit::RCPtr<AnoxEntityDefResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxEntityDefLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
