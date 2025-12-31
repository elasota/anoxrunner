#include "AnoxEntityDefResource.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/MemoryStream.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/VectorTrait.h"

#include "anox/Data/EntityDef.h"

#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

#include "AnoxAbstractSingleFileResource.h"

namespace anox
{
	class AnoxEntityDefResource;
	struct AnoxEntityDefResourceLoaderState;

	struct AnoxEntityDefResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
	};

	struct AnoxEntityDefLoaderInfo
	{
		typedef AnoxEntityDefResourceLoaderBase LoaderBase_t;
		typedef AnoxEntityDefResource Resource_t;
		typedef AnoxEntityDefResourceLoaderState State_t;

		static constexpr bool kHasDependencies = true;
		static constexpr bool kHasAnalysisPhase = true;

		static rkit::Result AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
	};

	class AnoxEntityDefResource final : public AnoxEntityDefResourceBase
	{
	public:
		const data::UserEntityDef &GetEntityDef() const override;

		data::UserEntityDef &ModifyUserEntityDef();

	private:
		data::UserEntityDef m_userEntityDef;
	};

	const data::UserEntityDef &AnoxEntityDefResource::GetEntityDef() const
	{
		return m_userEntityDef;
	}

	data::UserEntityDef &AnoxEntityDefResource::ModifyUserEntityDef()
	{
		return m_userEntityDef;
	}

	rkit::Result AnoxEntityDefLoaderInfo::AnalyzeFile(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		rkit::ReadOnlyMemoryStream stream(state.m_fileContents.ToSpan());

		data::UserEntityDef &edef = resource.ModifyUserEntityDef();

		RKIT_CHECK(stream.ReadOneBinary(edef));

		{
			rkit::RCPtr<rkit::Job> job;
			rkit::Future<AnoxResourceRetrieveResult> result;
			RKIT_CHECK(state.m_systems.m_resManager->GetContentIDKeyedResource(&job, result, resloaders::kMDAModelResourceTypeCode, edef.m_modelContentID));

			RKIT_CHECK(outDeps.AppendRValue(std::move(job)));
		}

		return rkit::ResultCode::kOK;
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
