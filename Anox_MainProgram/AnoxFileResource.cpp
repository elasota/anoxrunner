#include "AnoxFileResource.h"
#include "AnoxLoadEntireFileJob.h"
#include "AnoxGameFileSystem.h"
#include "AnoxLoadEntireFileJob.h"

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
	// Open -> Post IO Job         (Signaller) -> IO Complete
	//             `-----> IO Request --^
	class AnoxFileResourceLoader;

	class AnoxFileResource : public AnoxFileResourceBase
	{
	public:
		friend class AnoxFileResourceLoader;

		AnoxFileResource();

		rkit::Span<const uint8_t> GetContents() const override;

	private:
		rkit::Vector<uint8_t> m_fileBytes;
	};

	class AnoxFileResourceLoader final : public AnoxFileResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) const override;
	};

	AnoxFileResource::AnoxFileResource()
	{
	}

	rkit::Span<const uint8_t> AnoxFileResource::GetContents() const
	{
		return m_fileBytes.ToSpan();
	}

	rkit::Result AnoxFileResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxFileResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::RCPtr<rkit::Job> openJob;

		rkit::CIPath loosePath;
		RKIT_CHECK(loosePath.AppendComponent("loose"));
		RKIT_CHECK(loosePath.Append(key));

		rkit::RCPtr<rkit::Vector<uint8_t>> resourceBufferRCPtr = resource.StaticCast<AnoxFileResource>().FieldRef(&AnoxFileResource::m_fileBytes);

		return CreateLoadEntireFileJob(outJob, resourceBufferRCPtr, *systems.m_fileSystem, loosePath);
	}

	rkit::Result AnoxFileResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxFileResourceBase> &outResource) const
	{
		return rkit::New<AnoxFileResource>(outResource);
	}

	rkit::Result AnoxFileResourceLoaderBase::Create(rkit::RCPtr<AnoxFileResourceLoaderBase> &outFactory)
	{
		rkit::RCPtr<AnoxFileResourceLoader> resLoaderFactory;
		RKIT_CHECK(rkit::New<AnoxFileResourceLoader>(resLoaderFactory));

		outFactory = std::move(resLoaderFactory);

		return rkit::ResultCode::kOK;
	}
}
