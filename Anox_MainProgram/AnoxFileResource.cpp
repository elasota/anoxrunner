#include "AnoxFileResource.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Vector.h"

namespace anox
{
	class AnoxFileResource : public AnoxFileResourceBase
	{
	public:
		AnoxFileResource();

		rkit::Span<const uint8_t> GetContents() const override;

	private:
		rkit::Vector<uint8_t> m_fileBytes;
	};

	class AnoxFileResourceLoader final : public AnoxFileResourceLoaderBase
	{
	public:
		rkit::Result CreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob) override;
		rkit::Result RunProcessingTask(AnoxResourceBase &resource, const rkit::CIPathView &key) override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) override;
	};

	AnoxFileResource::AnoxFileResource()
	{
	}

	rkit::Span<const uint8_t> AnoxFileResource::GetContents() const
	{
		return m_fileBytes.ToSpan();
	}

	rkit::Result AnoxFileResourceLoader::CreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &key, rkit::RCPtr<rkit::Job> &outJob)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxFileResourceLoader::RunProcessingTask(AnoxResourceBase &resource, const rkit::CIPathView &key)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxFileResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource)
	{
		return rkit::New<AnoxFileResource>(outResource);
	}

	rkit::Result AnoxFileResourceLoaderBase::Create(rkit::RCPtr<AnoxFileResourceLoaderBase> &outResLoader)
	{
		rkit::RCPtr<AnoxFileResourceLoader> resLoader;
		RKIT_CHECK(rkit::New<AnoxFileResourceLoader>(resLoader));

		outResLoader = std::move(resLoader);

		return rkit::ResultCode::kOK;
	}
}
