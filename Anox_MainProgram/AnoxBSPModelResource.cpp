#include "AnoxBSPModelResource.h"
#include "AnoxLoadEntireFileJob.h"

#include "AnoxGameFileSystem.h"

#include "rkit/Core/FutureProtos.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
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
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxBSPModelResourceBase> &outResource) const override;
	};

	struct AnoxBSPModelResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_bspFileContents;
	};

	class AnoxBSPLoaderProcessJob final : public rkit::IJobRunner
	{
	public:
		explicit AnoxBSPLoaderProcessJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, const rkit::RCPtr<AnoxBSPModelResourceLoaderState> &state);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxBSPModelResourceBase> m_resource;
		rkit::RCPtr<AnoxBSPModelResourceLoaderState> m_state;
	};

	rkit::Result AnoxBSPModelResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::RCPtr<AnoxBSPModelResourceLoaderState> loaderState;
		RKIT_CHECK(rkit::New<AnoxBSPModelResourceLoaderState>(loaderState));


		rkit::RCPtr<rkit::Job> loadFileJob;
		RKIT_CHECK(CreateLoadEntireFileJob(loadFileJob, loaderState.FieldRef(&AnoxBSPModelResourceLoaderState::m_bspFileContents), fileSystem, key));

		rkit::UniquePtr<rkit::IJobRunner> processJobRunner;
		RKIT_CHECK(rkit::New<AnoxBSPLoaderProcessJob>(processJobRunner, resource, loaderState));

		RKIT_CHECK(fileSystem.GetJobQueue().CreateJob(&outJob, rkit::JobType::kNormalPriority, std::move(processJobRunner), loadFileJob));

		return rkit::ResultCode::kOK;
	}

	AnoxBSPLoaderProcessJob::AnoxBSPLoaderProcessJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, const rkit::RCPtr<AnoxBSPModelResourceLoaderState> &state)
		: m_resource(resource)
		, m_state(state)
	{
	}

	rkit::Result AnoxBSPLoaderProcessJob::Run()
	{
		return rkit::ResultCode::kNotYetImplemented;
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
