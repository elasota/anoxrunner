#include "AnoxEntityDefResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/VectorTrait.h"

#include "anox/Data/EntityDef.h"
#include "anox/AnoxUtilitiesDriver.h"

#include "AnoxDataReader.h"
#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"
#include "anox/AnoxModule.h"

#include "AnoxAbstractSingleFileResource.h"

namespace anox
{
	class AnoxEntityDefResource;
	struct AnoxEntityDefResourceLoaderState;

	struct AnoxEntityDefResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		data::UserEntityDef m_edef;
		rkit::FilePos_t m_edefDataPos = 0;
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
		state.m_edefDataPos = stream.Tell();

		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(state.m_systems.m_resManager->GetContentIDKeyedResource(&job, result, resloaders::kMDAModelResourceTypeCode, edef.m_modelContentID));

			RKIT_ASSERT(job.IsValid());
			RKIT_CHECK(outDeps.AppendRValue(std::move(job)));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxEntityDefLoaderInfo::LoadFile(State_t &state, Resource_t &resource)
	{
		anox::IUtilitiesDriver *anoxUtils = static_cast<anox::IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, "Utilities"));

		AnoxEntityDefResource::Values &resValues = resource.m_values;

		resource.m_values.m_modelCodeFourCC = state.m_edef.m_modelCode.Get();

		RKIT_CHECK(DataReader::ReadCheckVec(resource.m_values.m_scale, state.m_edef.m_scale, 16));
		RKIT_CHECK(DataReader::ReadCheckEnum(resource.m_values.m_shadowType, state.m_edef.m_shadowType));
		RKIT_CHECK(DataReader::ReadCheckVec(resource.m_values.m_bboxMin, state.m_edef.m_bboxMin, 16));
		RKIT_CHECK(DataReader::ReadCheckVec(resource.m_values.m_bboxMax, state.m_edef.m_bboxMax, 16));
		RKIT_CHECK(DataReader::ReadCheckEnumMask(resource.m_values.m_userEntityFlags, state.m_edef.m_flags));
		RKIT_CHECK(DataReader::ReadCheckFloat(resource.m_values.m_walkSpeed, state.m_edef.m_walkSpeed, 16));
		RKIT_CHECK(DataReader::ReadCheckFloat(resource.m_values.m_runSpeed, state.m_edef.m_runSpeed, 16));
		RKIT_CHECK(DataReader::ReadCheckFloat(resource.m_values.m_speed, state.m_edef.m_speed, 16));
		RKIT_CHECK(DataReader::ReadCheckLabel(resource.m_values.m_targetSequence, state.m_edef.m_targetSequenceID));
		RKIT_CHECK(DataReader::ReadCheckLabel(resource.m_values.m_startSequence, state.m_edef.m_startSequenceID));
		resource.m_values.m_miscValue = state.m_edef.m_miscValue.Get();

		rkit::Vector<char> descChars;
		RKIT_CHECK(descChars.Resize(state.m_edef.m_descriptionStringLength));

		rkit::ReadOnlyMemoryStream stream(state.m_fileContents.ToSpan());
		RKIT_CHECK(stream.SeekStart(state.m_edefDataPos));

		RKIT_CHECK(stream.ReadAllSpan(descChars.ToSpan()));

		RKIT_CHECK(DataReader::ReadCheckUTF8String(resource.m_values.m_description, descChars.ToSpan()));

		return rkit::ResultCode::kOK;
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
