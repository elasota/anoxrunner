#include "AnoxBSPModelResource.h"

#include "AnoxGameFileSystem.h"

#include "rkit/Core/FutureProtos.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/String.h"

namespace anox
{
	class AnoxBSPModelResource final : public AnoxBSPModelResourceBase
	{
	};

	class AnoxBSPModelResourceLoader final : public AnoxBSPModelResourceLoaderBase
	{
	public:
		rkit::Result CreateIOJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) override;
		rkit::Result RunProcessingTask(AnoxBSPModelResourceBase &resource, const rkit::CIPathView &key) override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxBSPModelResourceBase> &outResource) override;
	};

	rkit::Result AnoxBSPModelResourceLoader::CreateIOJob(const rkit::RCPtr<AnoxBSPModelResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxBSPModelResourceLoader::RunProcessingTask(AnoxBSPModelResourceBase &resource, const rkit::CIPathView &key)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxBSPModelResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxBSPModelResourceBase> &outResource)
	{
		return rkit::New<AnoxBSPModelResource>(outResource);
	}

	rkit::Result AnoxBSPModelResourceLoaderBase::Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &resLoader)
	{
		rkit::RCPtr<AnoxBSPModelResourceLoader> loader;
		RKIT_CHECK(rkit::New<AnoxBSPModelResourceLoader>(loader));

		resLoader = std::move(loader);

		return rkit::ResultCode::kOK;
	}
}
