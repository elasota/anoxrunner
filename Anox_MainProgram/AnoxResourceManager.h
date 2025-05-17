#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/StringProto.h"


namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IJobQueue;

	template<class T>
	class Future;

	class Job;
}

namespace rkit::data
{
	struct ContentID;
}

namespace anox::resloaders
{
	static const uint32_t kRawFileResourceTypeCode = RKIT_FOURCC('F', 'I', 'L', 'E');
}

namespace anox
{
	class AnoxGameFileSystemBase;
	class AnoxResourceManagerBase;
	class AnoxResourceLoaderSync;

	class AnoxResourceBase
	{
	public:
		virtual ~AnoxResourceBase() {}
	};

	struct AnoxResourceRetrieveResult
	{
		rkit::RCPtr<AnoxResourceBase> m_resourceHandle;
	};

	struct IAnoxResourceLoadCompletionNotifier
	{
		virtual void OnLoadCompleted() = 0;
	};

	struct AnoxResourceLoaderFactoryBase : public rkit::RefCounted
	{
		virtual ~AnoxResourceLoaderFactoryBase() {}

		virtual rkit::Result BaseCreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const void *keyPtr, rkit::RCPtr<rkit::Job> &outJob) = 0;
		virtual rkit::Result BaseRunProcessingTask(AnoxResourceBase &resource, const void *keyPtr) = 0;
		virtual rkit::Result BaseCreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) = 0;
	};

	template<class TKeyType>
	struct AnoxKeyedResourceLoader : public AnoxResourceLoaderFactoryBase
	{
	public:
		virtual rkit::Result Base2CreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) = 0;
		virtual rkit::Result Base2RunProcessingTask(AnoxResourceBase &resource, const TKeyType &key) = 0;

	private:
		rkit::Result BaseCreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const void *keyPtr, rkit::RCPtr<rkit::Job> &outJob) override
		{
			return this->Base2CreateIOJob(resource, fileSystem, *static_cast<const TKeyType *>(keyPtr), outJob);
		}

		rkit::Result BaseRunProcessingTask(AnoxResourceBase &resource, const void *keyPtr) override
		{
			return this->Base2RunProcessingTask(resource, *static_cast<const TKeyType *>(keyPtr));
		}
	};

	template<class TKeyType, class TResourceType>
	struct AnoxTypedResourceLoader : public AnoxKeyedResourceLoader<TKeyType>
	{
		virtual rkit::Result CreateIOJob(const rkit::RCPtr<TResourceType> &resource, AnoxGameFileSystemBase& fileSystem, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) = 0;
		virtual rkit::Result RunProcessingTask(TResourceType &resource, const TKeyType &key) = 0;
		virtual rkit::Result CreateResourceObject(rkit::UniquePtr<TResourceType> &outResource) = 0;

	private:
		rkit::Result Base2CreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, AnoxGameFileSystemBase &fileSystem, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) override
		{
			return this->CreateIOJob(resource.StaticCast<TResourceType>(), fileSystem, key, outJob);
		}

		rkit::Result Base2RunProcessingTask(AnoxResourceBase &resource, const TKeyType &key) override
		{
			return this->RunProcessingTask(static_cast<TResourceType &>(resource), key);
		}

		rkit::Result BaseCreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) override
		{
			rkit::UniquePtr<TResourceType> resource;
			RKIT_CHECK(this->CreateResourceObject(resource));

			outResource = std::move(resource);

			return rkit::ResultCode::kOK;
		}
	};

	template<class TResourceType>
	using AnoxStringKeyedResourceLoader = AnoxTypedResourceLoader<rkit::StringView, TResourceType> ;

	template<class TResourceType>
	using AnoxCIPathKeyedResourceLoader = AnoxTypedResourceLoader<rkit::CIPathView, TResourceType>;

	template<class TResourceType>
	using AnoxContentIDKeyedResourceLoader = AnoxTypedResourceLoader<rkit::data::ContentID, TResourceType>;

	enum class AnoxResourceKeyType
	{
		kInvalid,

		kContentID,
		kCIPath,
		kString,
	};

	class AnoxResourceManagerBase
	{
	public:
		virtual ~AnoxResourceManagerBase() {}

		virtual rkit::Result RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::data::ContentID>> &&factory) = 0;
		virtual rkit::Result RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::CIPathView>> &&factory) = 0;
		virtual rkit::Result RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::StringView>> &&factory) = 0;

		virtual rkit::Result GetContentIDKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) = 0;
		virtual rkit::Result GetCIPathKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) = 0;
		virtual rkit::Result GetStringKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxResourceManagerBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);
	};
}
