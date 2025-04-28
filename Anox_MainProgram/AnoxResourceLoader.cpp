#include "AnoxResourceLoader.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/JobQueue.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/UniquePtr.h"
#include "rkit/Core/String.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Future.h"
#include "rkit/Core/Path.h"
#include "rkit/Core/Vector.h"

#include "rkit/Data/ContentID.h"

namespace anox
{
	class AnoxResourceLoader;
	class AnoxResourceLoadFutureContainer;

	struct AnoxResourceLoaderSynchronizer final : public rkit::RefCounted
	{
	public:
		AnoxResourceLoaderSynchronizer(AnoxResourceLoader *loader);

		rkit::Result Init();

		rkit::UniquePtr<rkit::IMutex> m_resourcesMutex;
		AnoxResourceLoader *m_loader;
	};

	class AnoxResourceTracker : public rkit::RefCountedTracker
	{
	public:
		explicit AnoxResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource);

		virtual AnoxResourceKeyType GetKeyType() const = 0;

		// These are not synchronized, must be synced with "sync" resource mutex
		AnoxResourceBase *GetResource() const;

		rkit::Result GetOrCreateRetrieveResultFutureContainer(rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> &futureContainer);

		void SetLoadFuture(AnoxResourceLoadFutureContainer *container);
		void SetRegisteredByKey(bool isRegisteredByKey);
		bool IsRegisteredByKey() const;
		void SetResourceID(size_t resourceID);
		size_t GetResourceID() const;

		void DiscardFutureContainer();

		void SetAllocation(const rkit::SimpleObjectAllocation<AnoxResourceTracker> &self);

		void UnsyncedUnregisterResource();

		// These are self-synchronizing
		void OnLoadCompleted();
		void OnLoadFailed();

	private:
		void RCTrackerZero() override;

		rkit::UniquePtr<AnoxResourceBase> m_resource = nullptr;
		size_t m_resourceID = 0;
		bool m_isRegisteredByKey = false;

		AnoxResourceLoadFutureContainer *m_futureContainer;

		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;

		rkit::SimpleObjectAllocation<AnoxResourceTracker> m_self;
	};

	// The future tracker tracks the lifetime of a future container when
	// the resource isn't loaded yet.  This implicitly encompasses the lifetime
	// of the resource itself
	class AnoxResourceLoadFutureContainer final : public rkit::FutureContainer<AnoxResourceRetrieveResult>
	{
	public:
		explicit AnoxResourceLoadFutureContainer(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, AnoxResourceTracker *resourceTracker);
		~AnoxResourceLoadFutureContainer();

		void OnLoadCompleted();
		void OnLoadFailed();

	private:
		void RCTrackerZero() override;

		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;
		AnoxResourceTracker *m_resourceTracker;
	};

	class AnoxResourceLoadCompletionNotifier : public rkit::RefCounted, public IAnoxResourceLoadCompletionNotifier
	{
	public:
		explicit AnoxResourceLoadCompletionNotifier(const rkit::RCPtr<AnoxResourceLoadFutureContainer> &futureContainer);
		~AnoxResourceLoadCompletionNotifier();

		void OnLoadCompleted() override;

	private:
		rkit::RCPtr<AnoxResourceLoadFutureContainer> m_futureContainer;
		bool m_completed = false;
	};

	class StringKeyedResourceTracker : public AnoxResourceTracker
	{
	public:
		StringKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource);

		rkit::Result SetKey(const rkit::StringView &str);
		rkit::StringView GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::String m_key;
	};

	class ContentIDKeyedResourceTracker : public AnoxResourceTracker
	{
	public:
		ContentIDKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource);

		rkit::Result SetKey(const rkit::data::ContentID &cid);
		const rkit::data::ContentID &GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::data::ContentID m_key;
	};

	class CIPathKeyedResourceTracker : public AnoxResourceTracker
	{
	public:
		CIPathKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource);

		rkit::Result SetKey(const rkit::CIPathView &cid);
		rkit::CIPathView GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::CIPath m_key;
	};

	class AnoxResourceLoader final : public AnoxResourceLoaderBase
	{
	public:
		AnoxResourceLoader(AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);

		rkit::Result Initialize();

		rkit::Result RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryContentKeyed> &&factory) override;
		rkit::Result RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryCIPathKeyed> &&factory) override;
		rkit::Result RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryStringKeyed> &&factory) override;

		rkit::Result GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) override;
		rkit::Result GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) override;
		rkit::Result GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) override;

		void UnsyncedUnregisterResource(const AnoxResourceTracker *tracker);

	private:
		struct TypeKeyedFactory
		{
			AnoxResourceKeyType m_keyType = AnoxResourceKeyType::kInvalid;
			rkit::UniquePtr<IAnoxResourceLoaderFactoryBase> m_loaderFactory;
		};

		rkit::Result InternalRegisterLoaderFactory(uint32_t resourceType, AnoxResourceKeyType keyType, rkit::UniquePtr<IAnoxResourceLoaderFactoryBase> &&factory);

		template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType, class TFactoryType>
		rkit::Result InternalGetResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const TKeyViewType &key, rkit::HashMap<TKeyViewType, size_t> *resourceMap);

		AnoxGameFileSystemBase *m_fileSystem;
		rkit::IJobQueue *m_jobQueue;

		rkit::HashMap<uint32_t, TypeKeyedFactory> m_factories;
		rkit::UniquePtr<rkit::IMutex> m_factoryMutex;

		rkit::HashMap<rkit::CIPathView, size_t> m_pathKeyedResources;
		rkit::HashMap<rkit::StringView, size_t> m_stringKeyedResources;
		rkit::HashMap<rkit::data::ContentID, size_t> m_contentKeyedResources;
		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;

		rkit::Vector<AnoxResourceTracker*> m_resources;

		rkit::Vector<size_t> m_freeResourceIDs;
	};

	AnoxResourceLoaderSynchronizer::AnoxResourceLoaderSynchronizer(AnoxResourceLoader *loader)
		: m_loader(loader)
	{
	}


	rkit::Result AnoxResourceLoaderSynchronizer::Init()
	{
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->CreateMutex(m_resourcesMutex));

		return rkit::ResultCode::kOK;
	}


	AnoxResourceTracker::AnoxResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: RefCountedTracker(0)
		, m_sync(sync)
		, m_resource(std::move(resource))
	{
	}

	// These are not synchronized, must be synced with "sync" resource mutex
	AnoxResourceBase *AnoxResourceTracker::GetResource() const
	{
		return m_resource.Get();
	}

	rkit::Result AnoxResourceTracker::GetOrCreateRetrieveResultFutureContainer(rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> &futureContainer)
	{
		if (m_futureContainer)
		{
			futureContainer = rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>>(m_futureContainer);
		}
		else
		{
			// If the future container is gone, then the load operation completed.

			// Load either completed or failed
			RKIT_CHECK(rkit::New<rkit::FutureContainer<AnoxResourceRetrieveResult>>(futureContainer));

			if (m_resource.Get())
			{
				AnoxResourceRetrieveResult retrieveResult;
				retrieveResult.m_resourceHandle = rkit::RCPtr<AnoxResourceBase>(m_resource.Get(), this);
				futureContainer->Complete(retrieveResult);
			}
			else
			{
				futureContainer->Fail();
			}
		}

		return rkit::ResultCode::kOK;
	}

	void AnoxResourceTracker::SetLoadFuture(AnoxResourceLoadFutureContainer *container)
	{
		m_futureContainer = container;
	}

	void AnoxResourceTracker::SetRegisteredByKey(bool isRegisteredByKey)
	{
		m_isRegisteredByKey = true;
	}

	bool AnoxResourceTracker::IsRegisteredByKey() const
	{
		return m_isRegisteredByKey;
	}

	void AnoxResourceTracker::SetResourceID(size_t resourceID)
	{
		m_resourceID = resourceID;
	}

	size_t AnoxResourceTracker::GetResourceID() const
	{
		return m_resourceID;
	}

	void AnoxResourceTracker::DiscardFutureContainer()
	{
		m_futureContainer = nullptr;
	}

	void AnoxResourceTracker::RCTrackerZero()
	{
		if (m_resourceID == 0)
			return;

		{
			rkit::MutexLock lock(*m_sync->m_resourcesMutex);

			UnsyncedUnregisterResource();
		}

		rkit::Delete(m_self);
	}

	void AnoxResourceTracker::UnsyncedUnregisterResource()
	{
		if (m_resourceID == 0)
			return;

		if (m_sync->m_loader != nullptr)
			m_sync->m_loader->UnsyncedUnregisterResource(this);

		m_resourceID = 0;
	}

	void AnoxResourceTracker::OnLoadCompleted()
	{
		rkit::MutexLock lock(*m_sync->m_resourcesMutex);

		if (m_futureContainer)
		{
			AnoxResourceRetrieveResult result;
			result.m_resourceHandle = rkit::RCPtr<AnoxResourceBase>(m_resource.Get(), this);
			
			m_futureContainer->Complete(result);
		}
	}

	void AnoxResourceTracker::OnLoadFailed()
	{
		rkit::MutexLock lock(*m_sync->m_resourcesMutex);

		if (m_futureContainer)
			m_futureContainer->Fail();
	}

	AnoxResourceLoadFutureContainer::AnoxResourceLoadFutureContainer(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, AnoxResourceTracker *resourceTracker)
		: m_sync(sync)
		, m_resourceTracker(resourceTracker)
	{
		resourceTracker->RCTrackerAddRef();
	}

	AnoxResourceLoadFutureContainer::~AnoxResourceLoadFutureContainer()
	{
		m_resourceTracker->RCTrackerDecRef();
	}

	void AnoxResourceLoadFutureContainer::OnLoadCompleted()
	{
		m_resourceTracker->OnLoadCompleted();
		m_resourceTracker->RCTrackerDecRef();
		m_resourceTracker = nullptr;
	}

	void AnoxResourceLoadFutureContainer::OnLoadFailed()
	{
		m_resourceTracker->OnLoadFailed();
		m_resourceTracker->RCTrackerDecRef();
		m_resourceTracker = nullptr;
	}

	void AnoxResourceLoadFutureContainer::RCTrackerZero()
	{
		{
			rkit::MutexLock lock(*m_sync->m_resourcesMutex);

			// Check for possible object resurrection from InternalGetResource
			if (this->RCTrackerRefCount() != 0)
				return;

			// If this hits zero, then the load completion notifier is also gone,
			// and there should be no more possible references to the resource.
			m_resourceTracker->DiscardFutureContainer();

			m_resourceTracker->UnsyncedUnregisterResource();
		}

		// Release the hard ref to the resource tracker.  If the future didn't complete,
		// then this should be the only hard ref to the resource.
		m_resourceTracker->RCTrackerDecRef();

		FutureContainer<AnoxResourceRetrieveResult>::RCTrackerZero();
	}

	AnoxResourceLoadCompletionNotifier::AnoxResourceLoadCompletionNotifier(const rkit::RCPtr<AnoxResourceLoadFutureContainer> &futureContainer)
		: m_futureContainer(futureContainer)
	{
	}

	AnoxResourceLoadCompletionNotifier::~AnoxResourceLoadCompletionNotifier()
	{
		if (m_futureContainer)
			m_futureContainer->OnLoadFailed();
	}

	void AnoxResourceLoadCompletionNotifier::OnLoadCompleted()
	{
		m_futureContainer->OnLoadCompleted();
		m_futureContainer.Reset();
	}

	CIPathKeyedResourceTracker::CIPathKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, std::move(resource))
	{
	}

	rkit::Result CIPathKeyedResourceTracker::SetKey(const rkit::CIPathView &path)
	{
		return m_key.Set(path);
	}

	rkit::CIPathView CIPathKeyedResourceTracker::GetKey() const
	{
		return m_key;
	}

	AnoxResourceKeyType CIPathKeyedResourceTracker::GetKeyType() const
	{
		return AnoxResourceKeyType::kCIPath;
	}

	StringKeyedResourceTracker::StringKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, std::move(resource))
	{
	}

	rkit::Result StringKeyedResourceTracker::SetKey(const rkit::StringView &str)
	{
		return m_key.Set(str);
	}

	rkit::StringView StringKeyedResourceTracker::GetKey() const
	{
		return m_key;
	}

	AnoxResourceKeyType StringKeyedResourceTracker::GetKeyType() const
	{
		return AnoxResourceKeyType::kString;
	}

	ContentIDKeyedResourceTracker::ContentIDKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, std::move(resource))
	{
	}

	rkit::Result ContentIDKeyedResourceTracker::SetKey(const rkit::data::ContentID &cid)
	{
		m_key = cid;
		return rkit::ResultCode::kOK;
	}

	const rkit::data::ContentID &ContentIDKeyedResourceTracker::GetKey() const
	{
		return m_key;
	}

	AnoxResourceKeyType ContentIDKeyedResourceTracker::GetKeyType() const
	{
		return AnoxResourceKeyType::kContentID;
	}

	void AnoxResourceTracker::SetAllocation(const rkit::SimpleObjectAllocation<AnoxResourceTracker> &self)
	{
		m_self = self;
	}

	AnoxResourceLoader::AnoxResourceLoader(AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue)
		: m_fileSystem(fileSystem)
		, m_jobQueue(jobQueue)
	{
	}

	rkit::Result AnoxResourceLoader::Initialize()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(rkit::New<AnoxResourceLoaderSynchronizer>(m_sync, this));

		RKIT_CHECK(m_sync->Init());

		// Reserve resource ID zero as invalid
		RKIT_CHECK(m_resources.Append(nullptr));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxResourceLoader::RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryContentKeyed> &&factory)
	{
		return InternalRegisterLoaderFactory(resourceType, AnoxResourceKeyType::kContentID, std::move(factory));
	}

	rkit::Result AnoxResourceLoader::RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryCIPathKeyed> &&factory)
	{
		return InternalRegisterLoaderFactory(resourceType, AnoxResourceKeyType::kCIPath, std::move(factory));
	}

	rkit::Result AnoxResourceLoader::RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::UniquePtr<IAnoxResourceLoaderFactoryStringKeyed> &&factory)
	{
		return InternalRegisterLoaderFactory(resourceType, AnoxResourceKeyType::kString, std::move(factory));
	}

	rkit::Result AnoxResourceLoader::InternalRegisterLoaderFactory(uint32_t resourceType, AnoxResourceKeyType keyType, rkit::UniquePtr<IAnoxResourceLoaderFactoryBase> &&factory)
	{
		rkit::MutexLock lock(*m_factoryMutex);

		TypeKeyedFactory keyedFactory;
		keyedFactory.m_keyType = keyType;
		keyedFactory.m_loaderFactory = std::move(factory);

		RKIT_CHECK(m_factories.Set(resourceType, std::move(keyedFactory)));

		return rkit::ResultCode::kOK;
	}

	template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType, class TFactoryType>
	rkit::Result InternalUnsyncedFindExistingResource(bool &outFoundResource, rkit::Future<AnoxResourceRetrieveResult> &outFuture, rkit::MutexLock &mutexLock, uint32_t resourceType, const TKeyViewType &key, rkit::HashMap<TKeyViewType, size_t> *resourceMap)
	{

		outFoundResource = false;
		return rkit::ResultCode::kOK;
	}

	template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType, class TFactoryType>
	rkit::Result AnoxResourceLoader::InternalGetResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const TKeyViewType &key, rkit::HashMap<TKeyViewType, size_t> *resourceMap)
	{
		loadFuture.Reset();

		typedef typename rkit::HashMap<TKeyViewType, size_t>::ConstIterator_t MapConstIterator_t;
		typedef typename rkit::HashMap<TKeyViewType, size_t>::Iterator_t MapIterator_t;

		// This must go before the resource lock so that if it is destroyed, we don't deadlock
		// on the resource lock that is still alive in this scope!
		rkit::RCPtr<AnoxResourceLoadFutureContainer> loadFutureContainer;

		// See if the resource is already registered
		rkit::MutexLock resLock(*m_sync->m_resourcesMutex);

		typedef typename rkit::HashMap<TKeyViewType, size_t>::ConstIterator_t MapConstIterator_t;
		typedef typename rkit::HashMap<TKeyViewType, size_t>::Iterator_t MapIterator_t;

		MapIterator_t it = resourceMap->Find(key);
		if (it != resourceMap->end())
		{
			// Resource is registered
			AnoxResourceTracker *tracker = m_resources[it.Value()];

			AnoxResourceBase *resourcePtr = tracker->GetResource();
			if (resourcePtr)
			{
				// Resource is loaded
				rkit::RCPtr<AnoxResourceBase> trackerPtr = rkit::RCPtr<AnoxResourceBase>(resourcePtr, tracker);
				resLock.Unlock();

				rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> futureContainer;
				RKIT_CHECK(rkit::New<rkit::FutureContainer<AnoxResourceRetrieveResult>>(futureContainer));

				AnoxResourceRetrieveResult retrieveResult;
				retrieveResult.m_resourceHandle = rkit::RCPtr<AnoxResourceBase>(resourcePtr, tracker);
				futureContainer->Complete(std::move(retrieveResult));

				loadFuture = rkit::Future<AnoxResourceRetrieveResult>(futureContainer);
				return rkit::ResultCode::kOK;
			}
			else
			{
				// Resource is not loaded
				rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> futureContainer;
				RKIT_CHECK(tracker->GetOrCreateRetrieveResultFutureContainer(futureContainer));
				resLock.Unlock();

				loadFuture = rkit::Future<AnoxResourceRetrieveResult>(futureContainer);
				return rkit::ResultCode::kOK;
			}
		}

		// Resource is not registered
		rkit::MutexLock factoryLock(*m_factoryMutex);

		IAnoxResourceLoaderFactoryBase *factory = nullptr;

		rkit::UniquePtr<AnoxResourceBase> resource;

		rkit::UniquePtr<AnoxResourceBase> resourceInstance;
		{
			rkit::HashMap<uint32_t, TypeKeyedFactory>::ConstIterator_t factoryIt = m_factories.Find(resourceType);
			if (factoryIt == m_factories.end() || factoryIt.Value().m_keyType != TKeyType)
				return rkit::ResultCode::kInvalidParameter;

			factory = factoryIt.Value().m_loaderFactory.Get();
			RKIT_CHECK(factory->CreateResourceObject(resource));
		}

		if (m_freeResourceIDs.Count() == 0)
		{
			RKIT_CHECK(m_freeResourceIDs.Append(m_resources.Count()));

			rkit::Result padResourcesListResult = m_resources.Append(nullptr);
			if (!rkit::utils::ResultIsOK(padResourcesListResult))
			{
				m_freeResourceIDs.ShrinkToSize(m_freeResourceIDs.Count() - 1);
				return padResourcesListResult;
			}
		}

		const size_t resID = m_freeResourceIDs[m_freeResourceIDs.Count() - 1];

		rkit::UniquePtr<TKeyedTracker> keyedResTrackerUPtr;
		RKIT_CHECK(rkit::New<TKeyedTracker>(keyedResTrackerUPtr, m_sync, std::move(resource)));

		TKeyedTracker *keyedResTracker = keyedResTrackerUPtr.Get();
		AnoxResourceTracker *resTrackerPtr = keyedResTracker;

		RKIT_CHECK(rkit::New<AnoxResourceLoadFutureContainer>(loadFutureContainer, m_sync, resTrackerPtr));

		// Load future container now has a hard ref to the res tracker
		resTrackerPtr->SetAllocation(keyedResTrackerUPtr.Detach());

		resTrackerPtr->SetResourceID(resID);

		m_freeResourceIDs.ShrinkToSize(m_freeResourceIDs.Count() - 1);
		m_resources[resID] = resTrackerPtr;

		// DO NOT create any objects in this scope past this point that, if destroyed, will lock the
		// resources lock!  Doing that will deadlock if an operation fails!

		// Now that the resource is registered, any failures further down the function will
		// automatically remove the resource
		RKIT_CHECK(keyedResTracker->SetKey(key));

		TKeyViewType keyView = keyedResTracker->GetKey();
		RKIT_CHECK(resourceMap->Set(keyView, resID));

		resTrackerPtr->SetRegisteredByKey(true);

		resLock.Unlock();

		rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> loadCompleter;
		RKIT_CHECK(rkit::New<AnoxResourceLoadCompletionNotifier>(loadCompleter, loadFutureContainer));

		RKIT_CHECK(factory->QueueLoad(this, std::move(loadCompleter)));
		loadCompleter.Reset();

		factoryLock.Unlock();

		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> recastContainer;
		recastContainer = std::move(loadFutureContainer);

		loadFuture = rkit::Future<AnoxResourceRetrieveResult>(recastContainer);

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxResourceLoader::GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		return InternalGetResource<ContentIDKeyedResourceTracker, rkit::data::ContentID, AnoxResourceKeyType::kContentID, IAnoxResourceLoaderFactoryContentKeyed>(loadFuture, resourceType, cid, &m_contentKeyedResources);
	}

	rkit::Result AnoxResourceLoader::GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path)
	{
		return InternalGetResource<CIPathKeyedResourceTracker, rkit::CIPathView, AnoxResourceKeyType::kCIPath, IAnoxResourceLoaderFactoryCIPathKeyed>(loadFuture, resourceType, path, &m_pathKeyedResources);
	}

	rkit::Result AnoxResourceLoader::GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str)
	{
		return InternalGetResource<StringKeyedResourceTracker, rkit::StringView, AnoxResourceKeyType::kString, IAnoxResourceLoaderFactoryStringKeyed>(loadFuture, resourceType, str, &m_stringKeyedResources);
	}

	void AnoxResourceLoader::UnsyncedUnregisterResource(const AnoxResourceTracker *tracker)
	{
		switch (tracker->GetKeyType())
		{
		case AnoxResourceKeyType::kCIPath:
			m_pathKeyedResources.Remove(static_cast<const CIPathKeyedResourceTracker *>(tracker)->GetKey());
			break;

		case AnoxResourceKeyType::kContentID:
			m_contentKeyedResources.Remove(static_cast<const ContentIDKeyedResourceTracker *>(tracker)->GetKey());
			break;

		case AnoxResourceKeyType::kString:
			m_stringKeyedResources.Remove(static_cast<const StringKeyedResourceTracker *>(tracker)->GetKey());
			break;
		default:
			break;
		}
	}

	rkit::Result AnoxResourceLoaderBase::Create(rkit::UniquePtr<AnoxResourceLoaderBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue)
	{
		rkit::UniquePtr<AnoxResourceLoader> resLoader;

		RKIT_CHECK(rkit::New<AnoxResourceLoader>(resLoader, fileSystem, jobQueue));
		RKIT_CHECK(resLoader->Initialize());

		outResLoader = std::move(resLoader);

		return rkit::ResultCode::kOK;
	}
}
