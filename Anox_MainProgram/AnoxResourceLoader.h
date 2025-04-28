#pragma once

#include "rkit/Core/CoreDefs.h"
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
}

namespace rkit::data
{
	struct ContentID;
}

namespace anox
{
	class AnoxGameFileSystemBase;
	class AnoxResourceLoaderBase;
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

	struct IAnoxResourceLoaderFactoryBase
	{
		virtual ~IAnoxResourceLoaderFactoryBase() {}

		virtual rkit::Result QueueLoad(AnoxResourceLoaderBase *loader, rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> &&completionNotifier) = 0;
		virtual rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxResourceBase> &outResource) = 0;
	};

	struct IAnoxResourceLoaderFactoryContentKeyed : public IAnoxResourceLoaderFactoryBase
	{
		virtual rkit::Result CreateLoader(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, const rkit::data::ContentID &contentID) = 0;
	};

	struct IAnoxResourceLoaderFactoryCIPathKeyed : public IAnoxResourceLoaderFactoryBase
	{
		virtual rkit::Result CreateLoader(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, const rkit::CIPathView &pathKey) = 0;
	};

	struct IAnoxResourceLoaderFactoryStringKeyed : public IAnoxResourceLoaderFactoryBase
	{
		virtual rkit::Result CreateLoader(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, const rkit::StringView &pathKey) = 0;
	};

	enum class AnoxResourceKeyType
	{
		kInvalid,

		kContentID,
		kCIPath,
		kString,
	};

	class AnoxResourceLoaderBase
	{
	public:
		virtual rkit::Result RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryContentKeyed> &&factory) = 0;
		virtual rkit::Result RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryCIPathKeyed> &&factory) = 0;
		virtual rkit::Result RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryStringKeyed> &&factory) = 0;

		virtual rkit::Result GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) = 0;
		virtual rkit::Result GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) = 0;
		virtual rkit::Result GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxResourceLoaderBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);
	};
}
