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

		virtual rkit::Result BaseCreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxGameFileSystemBase &fileSystem, const void *keyPtr, rkit::RCPtr<rkit::Job> &outJob) = 0;
		virtual rkit::Result BaseRunProcessingTask(AnoxResourceBase &resource, const void *keyPtr) = 0;
		virtual rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) = 0;
	};

	template<class TKeyType>
	struct AnoxKeyedResourceLoader : public AnoxResourceLoaderFactoryBase
	{
		virtual rkit::Result CreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxGameFileSystemBase& fileSystem, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) = 0;
		virtual rkit::Result RunProcessingTask(AnoxResourceBase &resource, const TKeyType &key) = 0;

	private:
		rkit::Result BaseCreateIOJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxGameFileSystemBase &fileSystem, const void *keyPtr, rkit::RCPtr<rkit::Job> &outJob) override
		{
			return this->CreateIOJob(resource, fileSystem, *static_cast<const TKeyType *>(keyPtr), outJob);
		}

		rkit::Result BaseRunProcessingTask(AnoxResourceBase &resource, const void *keyPtr) override
		{
			return this->RunProcessingTask(resource, *static_cast<const TKeyType *>(keyPtr));
		}
	};

	typedef AnoxKeyedResourceLoader<rkit::StringView> AnoxStringKeyedResourceLoader;
	typedef AnoxKeyedResourceLoader<rkit::CIPathView> AnoxCIPathKeyedResourceLoader;
	typedef AnoxKeyedResourceLoader<rkit::data::ContentID> AnoxContentIDKeyedResourceLoader;

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
		virtual rkit::Result RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxContentIDKeyedResourceLoader> &&factory) = 0;
		virtual rkit::Result RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxCIPathKeyedResourceLoader> &&factory) = 0;
		virtual rkit::Result RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxStringKeyedResourceLoader> &&factory) = 0;

		virtual rkit::Result GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) = 0;
		virtual rkit::Result GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) = 0;
		virtual rkit::Result GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxResourceManagerBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);
	};
}
