#include "AnoxFileResource.h"
#include "AnoxLoadEntireFileJob.h"
#include "AnoxGameFileSystem.h"

#include "rkit/Core/FutureProtos.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/AsyncFile.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	// Job chart:
	// Open -> Post IO Job         (Signaler) -> IO Complete
	//             `-----> IO Request --^
	class AnoxPathFileResourceLoader;
	class AnoxContentFileResourceLoader;

	class AnoxFileResource : public AnoxFileResourceBase
	{
	public:
		friend class AnoxPathFileResourceLoader;
		friend class AnoxContentFileResourceLoader;

		AnoxFileResource();

		rkit::Span<const uint8_t> GetContents() const override;

	private:
		rkit::Vector<uint8_t> m_fileBytes;
	};

	class AnoxPathFileResourceLoader final : public AnoxPathFileResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) const override;
	};

	class AnoxContentFileResourceLoader final : public AnoxContentFileResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) const override;
	};

	AnoxFileResource::AnoxFileResource()
	{
	}

	rkit::Span<const uint8_t> AnoxFileResource::GetContents() const
	{
		return m_fileBytes.ToSpan();
	}

	rkit::Result AnoxPathFileResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::RCPtr<rkit::Job> openJob;

		rkit::RCPtr<rkit::Vector<uint8_t>> resourceBufferRCPtr = resource.StaticCast<AnoxFileResource>().FieldRef(&AnoxFileResource::m_fileBytes);

		return CreateLoadEntireFileJob(outJob, resourceBufferRCPtr, *systems.m_fileSystem, key);
	}

	rkit::Result AnoxPathFileResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) const
	{
		return rkit::New<AnoxFileResource>(outResource);
	}

	rkit::Result AnoxPathFileResourceLoaderBase::Create(rkit::RCPtr<AnoxPathFileResourceLoaderBase> &outFactory)
	{
		rkit::RCPtr<AnoxPathFileResourceLoader> resLoaderFactory;
		RKIT_CHECK(rkit::New<AnoxPathFileResourceLoader>(resLoaderFactory));

		outFactory = std::move(resLoaderFactory);

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxContentFileResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::RCPtr<rkit::Job> openJob;

		rkit::RCPtr<rkit::Vector<uint8_t>> resourceBufferRCPtr = resource.StaticCast<AnoxFileResource>().FieldRef(&AnoxFileResource::m_fileBytes);

		return CreateLoadEntireFileJob(outJob, resourceBufferRCPtr, *systems.m_fileSystem, key);
	}

	rkit::Result AnoxContentFileResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) const
	{
		return rkit::New<AnoxFileResource>(outResource);
	}

	rkit::Result AnoxContentFileResourceLoaderBase::Create(rkit::RCPtr<AnoxContentFileResourceLoaderBase> &outFactory)
	{
		rkit::RCPtr<AnoxContentFileResourceLoader> resLoaderFactory;
		RKIT_CHECK(rkit::New<AnoxContentFileResourceLoader>(resLoaderFactory));

		outFactory = std::move(resLoaderFactory);

		RKIT_RETURN_OK;
	}
}
