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

	namespace data
	{
		struct ContentID;
	}
}

namespace anox
{
	namespace resloaders
	{
		static const uint32_t kRawFileResourceTypeCode = RKIT_FOURCC('F', 'I', 'L', 'E');
		static const uint32_t kBSPModelResourceTypeCode = RKIT_FOURCC('B', 'S', 'P', 'M');
		static const uint32_t kWorldMaterialTypeCode = RKIT_FOURCC('M', 'T', 'L', 'W');
		static const uint32_t kTextureResourceTypeCode = RKIT_FOURCC('T', 'X', 'T', 'R');
		static const uint32_t kEntityDefTypeCode = RKIT_FOURCC('E', 'D', 'E', 'F');
		static const uint32_t kMDAModelResourceTypeCode = RKIT_FOURCC('M', 'D', 'A', 'M');
	}
}

namespace anox
{
	class AnoxGameFileSystemBase;
	class AnoxResourceManagerBase;
	class AnoxResourceLoaderSync;
	struct IGraphicsSubsystem;

	struct IGraphicsResourceFactory;

	class AnoxResourceBase
	{
	public:
		virtual ~AnoxResourceBase() {}
	};

	struct AnoxResourceRetrieveResult
	{
		rkit::RCPtr<AnoxResourceBase> m_resourceHandle;
	};

	struct AnoxResourceLoaderSystems
	{
		AnoxResourceManagerBase *m_resManager = nullptr;
		AnoxGameFileSystemBase *m_fileSystem = nullptr;
		IGraphicsSubsystem *m_graphicsSystem = nullptr;
	};

	struct AnoxResourceLoaderBase : public rkit::RefCounted
	{
		virtual ~AnoxResourceLoaderBase() {}

		virtual rkit::Result BaseCreateLoadJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const void *keyPtr, rkit::RCPtr<rkit::Job> &outJob) const = 0;
		virtual rkit::Result BaseCreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) const = 0;
	};

	template<class TKeyType>
	struct AnoxKeyedResourceLoader : public AnoxResourceLoaderBase
	{
	public:
		virtual rkit::Result Base2CreateLoadJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) const = 0;

	private:
		rkit::Result BaseCreateLoadJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const void *keyPtr, rkit::RCPtr<rkit::Job> &outJob) const override
		{
			return this->Base2CreateLoadJob(resource, systems, *static_cast<const TKeyType *>(keyPtr), outJob);
		}
	};

	template<class TKeyType, class TResourceType>
	struct AnoxTypedResourceLoader : public AnoxKeyedResourceLoader<TKeyType>
	{
		typedef TKeyType KeyType_t;
		typedef TResourceType ResourceBase_t;

		virtual rkit::Result CreateLoadJob(const rkit::RCPtr<TResourceType> &resource, const AnoxResourceLoaderSystems &systems, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) const = 0;
		virtual rkit::Result CreateResourceObject(rkit::UniquePtr<TResourceType> &outResource) const = 0;

	private:
		rkit::Result Base2CreateLoadJob(const rkit::RCPtr<AnoxResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const TKeyType &key, rkit::RCPtr<rkit::Job> &outJob) const override
		{
			return this->CreateLoadJob(resource.StaticCast<TResourceType>(), systems, key, outJob);
		}

		rkit::Result BaseCreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) const override
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

		virtual rkit::Result RegisterContentKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::data::ContentID>> &&factory) = 0;
		virtual rkit::Result RegisterCIPathKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::CIPathView>> &&factory) = 0;
		virtual rkit::Result RegisterStringKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::StringView>> &&factory) = 0;

		virtual rkit::Result GetContentIDKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) = 0;
		virtual rkit::Result GetCIPathKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) = 0;
		virtual rkit::Result GetStringKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) = 0;

		virtual rkit::Result EnumerateCIPathKeyedResources(const rkit::CIPathView &basePath, rkit::Vector<rkit::CIPath> &outFiles, rkit::Vector<rkit::CIPath> &outDirectories) const = 0;

		virtual void SetGraphicsSubsystem(IGraphicsSubsystem *graphicsSubsystem) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxResourceManagerBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);
	};
}
