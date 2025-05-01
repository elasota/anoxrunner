#include "AnoxResourceManager.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Job.h"
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

#include "AnoxFileResource.h"

// NOTES ON OWNERSHIP AND REFCOUNT BEHAVIOR:
// Load Jobs -> AnoxResourceLoadCompletionNotifier -> AnoxResourceLoadFutureContainer -> AnoxResourceTracker
//


namespace anox
{
	class AnoxResourceManager;
	class AnoxPendingResourceFutureContainer;
	class AnoxIOJobRunner;

	struct AnoxResourceLoaderSynchronizer final : public rkit::RefCounted
	{
		AnoxResourceLoaderSynchronizer(AnoxResourceManager *resManager);

		rkit::Result Init();

		rkit::UniquePtr<rkit::IMutex> m_resourcesMutex;
		AnoxResourceManager *m_resManager;
	};

	struct AnoxResourceTracker : public rkit::RefCountedTracker
	{
		explicit AnoxResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource);

		virtual AnoxResourceKeyType GetKeyType() const = 0;

		void RCTrackerZero() override;

		void FailLoading();
		void SucceedLoading();

		rkit::UniquePtr<AnoxResourceBase> m_resource = nullptr;

		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> m_pendingFutureContainer;

		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;
		rkit::SimpleObjectAllocation<AnoxResourceTracker> m_self;

		AnoxResourceTracker *m_prevResource = nullptr;
		AnoxResourceTracker *m_nextResource = nullptr;
	};

	class AnoxResourceLoadCompletionNotifier : public rkit::RefCounted, public IAnoxResourceLoadCompletionNotifier
	{
	public:
		explicit AnoxResourceLoadCompletionNotifier(const rkit::RCPtr<AnoxResourceTracker> &resource);
		~AnoxResourceLoadCompletionNotifier();

		void OnLoadCompleted() override;

	private:
		rkit::RCPtr<AnoxResourceTracker> m_resource;
	};

	class StringKeyedResourceTracker : public AnoxResourceTracker
	{
	public:
		StringKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource);

		rkit::Result SetKey(const rkit::StringView &str);
		const rkit::StringView &GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::String m_key;
		rkit::StringView m_keyView;
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
		const rkit::CIPathView &GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::CIPath m_key;
		rkit::CIPathView m_keyView;
	};

	class AnoxIOJobRunner final : public rkit::IJobRunner
	{
	public:
		AnoxIOJobRunner(const rkit::RCPtr<AnoxResourceLoaderFactoryBase> &factory, AnoxResourceBase *resource,
			const rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> &loadCompleter, const void *keyPtr);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxResourceLoaderFactoryBase> m_factory;
		AnoxResourceBase *m_resource;
		rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> m_loadCompleter;
		const void *m_keyPtr;
	};

	class AnoxProcessJobRunner final : public rkit::IJobRunner
	{
	public:
		AnoxProcessJobRunner(const rkit::RCPtr<AnoxResourceLoaderFactoryBase> &factory, AnoxResourceBase *resource,
			const rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> &loadCompleter, const void *keyPtr);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxResourceLoaderFactoryBase> m_factory;
		AnoxResourceBase *m_resource;
		rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> m_loadCompleter;
		const void *m_keyPtr;
	};

	class AnoxProcessAndCompleteJobRunner final : public rkit::IJobRunner
	{
	public:
		AnoxProcessAndCompleteJobRunner();

		rkit::Result Run() override;
	};

	class AnoxResourceManager final : public AnoxResourceManagerBase
	{
	public:
		AnoxResourceManager(AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);

		rkit::Result Initialize();

		rkit::Result RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxContentIDKeyedResourceLoader> &&factory) override;
		rkit::Result RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxCIPathKeyedResourceLoader> &&factory) override;
		rkit::Result RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxStringKeyedResourceLoader> &&factory) override;

		rkit::Result GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) override;
		rkit::Result GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) override;
		rkit::Result GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) override;

		void UnsyncedUnregisterResource(const AnoxResourceTracker *tracker, bool wasLinked);

	private:
		struct TypeKeyedFactory
		{
			AnoxResourceKeyType m_keyType = AnoxResourceKeyType::kInvalid;
			rkit::RCPtr<AnoxResourceLoaderFactoryBase> m_loaderFactory;
		};

		rkit::Result InternalRegisterLoaderFactory(uint32_t resourceType, AnoxResourceKeyType keyType, rkit::RCPtr<AnoxResourceLoaderFactoryBase> &&factory);

		template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType>
		rkit::Result InternalGetResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const TKeyViewType &key, rkit::HashMap<TKeyViewType, AnoxResourceTracker *> *resourceMap);

		AnoxGameFileSystemBase *m_fileSystem;
		rkit::IJobQueue *m_jobQueue;

		rkit::HashMap<uint32_t, TypeKeyedFactory> m_factories;
		rkit::UniquePtr<rkit::IMutex> m_factoryMutex;

		rkit::HashMap<rkit::CIPathView, AnoxResourceTracker *> m_pathKeyedResources;
		rkit::HashMap<rkit::StringView, AnoxResourceTracker *> m_stringKeyedResources;
		rkit::HashMap<rkit::data::ContentID, AnoxResourceTracker *> m_contentKeyedResources;
		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;

		AnoxResourceTracker* m_firstResource = nullptr;
		AnoxResourceTracker* m_lastResource = nullptr;
	};

	AnoxResourceLoaderSynchronizer::AnoxResourceLoaderSynchronizer(AnoxResourceManager *resManager)
		: m_resManager(resManager)
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

	void AnoxResourceTracker::RCTrackerZero()
	{
		{
			rkit::MutexLock lock(*m_sync->m_resourcesMutex);

			// Check for object resurrection, which can happen if the object is being retrieved
			// from the resource manager while the last live reference is being destroyed
			if (RCTrackerRefCount() != 0)
				return;

			// Really gone, unregister the resource
			AnoxResourceManager *resManager = m_sync->m_resManager;

			const bool isLinked = (m_nextResource != nullptr || m_prevResource != nullptr);

			if (resManager)
				resManager->UnsyncedUnregisterResource(this, isLinked);

			if (m_prevResource)
				m_prevResource->m_nextResource = m_nextResource;

			if (m_nextResource)
				m_nextResource->m_prevResource = m_prevResource;
		}

		rkit::Delete(m_self);
	}

	void AnoxResourceTracker::SucceedLoading()
	{
		// Make sure we're keeping hard refs to the resource
		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> futureContainerRCPtr;
		AnoxResourceRetrieveResult result;

		{
			rkit::MutexLock lock(*m_sync->m_resourcesMutex);

			futureContainerRCPtr = std::move(m_pendingFutureContainer);
			m_pendingFutureContainer.Reset();

			result.m_resourceHandle = rkit::RCPtr<AnoxResourceBase>(m_resource.Get(), this);
		}

		futureContainerRCPtr->Complete(std::move(result));
		
	}

	AnoxResourceLoadCompletionNotifier::AnoxResourceLoadCompletionNotifier(const rkit::RCPtr<AnoxResourceTracker> &resource)
		: m_resource(resource)
	{
	}

	AnoxResourceLoadCompletionNotifier::~AnoxResourceLoadCompletionNotifier()
	{
	}

	void AnoxResourceLoadCompletionNotifier::OnLoadCompleted()
	{
		m_resource->SucceedLoading();
		m_resource.Reset();
	}

	CIPathKeyedResourceTracker::CIPathKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, std::move(resource))
	{
	}

	rkit::Result CIPathKeyedResourceTracker::SetKey(const rkit::CIPathView &path)
	{
		RKIT_CHECK(m_key.Set(path));
		m_keyView = m_key;

		return rkit::ResultCode::kOK;
	}

	const rkit::CIPathView &CIPathKeyedResourceTracker::GetKey() const
	{
		return m_keyView;
	}

	AnoxResourceKeyType CIPathKeyedResourceTracker::GetKeyType() const
	{
		return AnoxResourceKeyType::kCIPath;
	}

	AnoxIOJobRunner::AnoxIOJobRunner(const rkit::RCPtr<AnoxResourceLoaderFactoryBase> &factory, AnoxResourceBase *resource,
		const rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> &loadCompleter, const void *keyPtr)
		: m_factory(factory)
		, m_resource(resource)
		, m_loadCompleter(loadCompleter)
		, m_keyPtr(keyPtr)
	{
	}

	rkit::Result AnoxIOJobRunner::Run()
	{
		return m_factory->BaseRunIOTask(*m_resource, m_keyPtr);
	}

	AnoxProcessJobRunner::AnoxProcessJobRunner(const rkit::RCPtr<AnoxResourceLoaderFactoryBase> &factory, AnoxResourceBase *resource,
		const rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> &loadCompleter, const void *keyPtr)
		: m_factory(factory)
		, m_resource(resource)
		, m_loadCompleter(loadCompleter)
		, m_keyPtr(keyPtr)
	{
	}

	rkit::Result AnoxProcessJobRunner::Run()
	{
		return m_factory->BaseRunProcessingTask(*m_resource, m_keyPtr);
	}

	StringKeyedResourceTracker::StringKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, std::move(resource))
	{
	}

	rkit::Result StringKeyedResourceTracker::SetKey(const rkit::StringView &str)
	{
		RKIT_CHECK(m_key.Set(str));
		m_keyView = m_key;

		return rkit::ResultCode::kOK;
	}

	const rkit::StringView &StringKeyedResourceTracker::GetKey() const
	{
		return m_keyView;
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

	AnoxResourceManager::AnoxResourceManager(AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue)
		: m_fileSystem(fileSystem)
		, m_jobQueue(jobQueue)
	{
	}

	rkit::Result AnoxResourceManager::Initialize()
	{
		rkit::ISystemDriver *sysDriver = rkit::GetDrivers().m_systemDriver;

		RKIT_CHECK(rkit::New<AnoxResourceLoaderSynchronizer>(m_sync, this));
		RKIT_CHECK(sysDriver->CreateMutex(m_factoryMutex));

		RKIT_CHECK(m_sync->Init());

		rkit::RCPtr<AnoxFileResourceLoaderBase> fileLoader;
		RKIT_CHECK(AnoxFileResourceLoaderBase::Create(fileLoader));

		RKIT_CHECK(RegisterCIPathKeyedLoaderFactory(resloaders::kRawFileResourceTypeCode, std::move(fileLoader)));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxResourceManager::RegisterContentKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxContentIDKeyedResourceLoader> &&factory)
	{
		return InternalRegisterLoaderFactory(resourceType, AnoxResourceKeyType::kContentID, std::move(factory));
	}

	rkit::Result AnoxResourceManager::RegisterCIPathKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxCIPathKeyedResourceLoader> &&factory)
	{
		return InternalRegisterLoaderFactory(resourceType, AnoxResourceKeyType::kCIPath, std::move(factory));
	}

	rkit::Result AnoxResourceManager::RegisterStringKeyedLoaderFactory(uint32_t resourceType, rkit::RCPtr<AnoxStringKeyedResourceLoader> &&factory)
	{
		return InternalRegisterLoaderFactory(resourceType, AnoxResourceKeyType::kString, std::move(factory));
	}

	rkit::Result AnoxResourceManager::InternalRegisterLoaderFactory(uint32_t resourceType, AnoxResourceKeyType keyType, rkit::RCPtr<AnoxResourceLoaderFactoryBase> &&factory)
	{
		rkit::MutexLock lock(*m_factoryMutex);

		TypeKeyedFactory keyedFactory;
		keyedFactory.m_keyType = keyType;
		keyedFactory.m_loaderFactory = std::move(factory);

		RKIT_CHECK(m_factories.Set(resourceType, std::move(keyedFactory)));

		return rkit::ResultCode::kOK;
	}

	template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType>
	rkit::Result AnoxResourceManager::InternalGetResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const TKeyViewType &key, rkit::HashMap<TKeyViewType, AnoxResourceTracker *> *resourceMap)
	{
		loadFuture.Reset();

		typedef typename rkit::HashMap<TKeyViewType, AnoxResourceTracker *>::ConstIterator_t MapConstIterator_t;
		typedef typename rkit::HashMap<TKeyViewType, AnoxResourceTracker *>::Iterator_t MapIterator_t;

		// These must be before the lock so that if this function fails,
		// they will be destroyed outside of the lock, since their destruction
		// will lock the resource mutex
		rkit::RCPtr<AnoxResourceBase> resourceRCPtr;
		rkit::RCPtr<IAnoxResourceLoadCompletionNotifier> loadCompleter;

		rkit::MutexLock resLock(*m_sync->m_resourcesMutex);

		MapIterator_t it = resourceMap->Find(key);
		if (it != resourceMap->end())
		{
			// Resource is registered
			AnoxResourceTracker *tracker = it.Value();

			if (tracker->m_pendingFutureContainer.IsValid())
			{
				// Resource is not loaded
				loadFuture = rkit::Future<AnoxResourceRetrieveResult>(tracker->m_pendingFutureContainer);
				resLock.Unlock();

				return rkit::ResultCode::kOK;
			}
			else
			{
				// Resource either loaded or failed
				AnoxResourceBase *resourcePtr = tracker->m_resource.Get();

				rkit::RCPtr<AnoxResourceBase> trackerPtr;
				if (resourcePtr != nullptr)
				{
					trackerPtr = rkit::RCPtr<AnoxResourceBase>(resourcePtr, tracker);
				}

				resLock.Unlock();

				rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> futureContainer;
				RKIT_CHECK(rkit::New<rkit::FutureContainer<AnoxResourceRetrieveResult>>(futureContainer));

				if (trackerPtr.IsValid())
				{
					AnoxResourceRetrieveResult retrieveResult;
					retrieveResult.m_resourceHandle = rkit::RCPtr<AnoxResourceBase>(resourcePtr, tracker);
					futureContainer->Complete(std::move(retrieveResult));
				}
				else
					futureContainer->Fail();

				loadFuture = rkit::Future<AnoxResourceRetrieveResult>(futureContainer);
				return rkit::ResultCode::kOK;
			}
		}

		// Resource is not registered
		rkit::RCPtr<AnoxResourceLoaderFactoryBase> factory;
		{
			rkit::MutexLock factoryLock(*m_factoryMutex);
			rkit::HashMap<uint32_t, TypeKeyedFactory>::ConstIterator_t factoryIt = m_factories.Find(resourceType);
			if (factoryIt == m_factories.end() || factoryIt.Value().m_keyType != TKeyType)
				return rkit::ResultCode::kInvalidParameter;

			factory = factoryIt.Value().m_loaderFactory;
		}

		// Create the resource
		rkit::UniquePtr<AnoxResourceBase> resourceUPtr;
		RKIT_CHECK(factory->CreateResourceObject(resourceUPtr));

		AnoxResourceBase *resource = resourceUPtr.Get();

		rkit::UniquePtr<TKeyedTracker> keyedTrackerUPtr;
		RKIT_CHECK(rkit::New<TKeyedTracker>(keyedTrackerUPtr, m_sync, std::move(resourceUPtr)));

		// Create the tracker and form a RC ptr from it immediately.
		// We need to do this the unregister code is in RCTrackerZero, not
		// the destructor.
		TKeyedTracker *keyedTracker = keyedTrackerUPtr.Get();
		AnoxResourceTracker *tracker = keyedTracker;
		tracker->m_self = keyedTrackerUPtr.Detach();

		resourceRCPtr = rkit::RCPtr<AnoxResourceBase>(resource, tracker);

		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> pendingFutureContainer;
		RKIT_CHECK(rkit::New<rkit::FutureContainer<AnoxResourceRetrieveResult>>(pendingFutureContainer));

		tracker->m_pendingFutureContainer = pendingFutureContainer;

		// Create the completion notifier
		rkit::RCPtr<AnoxResourceTracker> trackerRCPtr = rkit::RCPtr<AnoxResourceTracker>(tracker, tracker);

		RKIT_CHECK(rkit::New<AnoxResourceLoadCompletionNotifier>(loadCompleter, trackerRCPtr));

		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> recastContainer;

		RKIT_CHECK(resourceMap->Set(keyedTracker->GetKey(), tracker));

		// With the load completion notifier set up, any further failures in this function
		// will trigger a load failure, and the resource is registered now, so we can add it
		// to the resource list and unlock.

		tracker->m_prevResource = m_lastResource;
		tracker->m_nextResource = nullptr;

		if (m_firstResource == nullptr)
			m_firstResource = tracker;
		if (m_lastResource)
			m_lastResource->m_nextResource = tracker;

		m_lastResource = tracker;

		// And now that the safeguards are set, we can unlock
		resLock.Unlock();

		// Add the load jobs
		rkit::RCPtr<rkit::Job> ioJob;
		const void *keyPtr = &keyedTracker->GetKey();

		{
			rkit::UniquePtr<rkit::IJobRunner> ioJobRunner;
			RKIT_CHECK(rkit::New<AnoxIOJobRunner>(ioJobRunner, factory, resource, loadCompleter, keyPtr));
			RKIT_CHECK(m_jobQueue->CreateJob(&ioJob, rkit::JobType::kIO, std::move(ioJobRunner), rkit::Span<rkit::RCPtr<rkit::Job>>().ToValueISpan()));
		}

		{
			rkit::Span<rkit::RCPtr<rkit::Job>> processJobDepSpan(&ioJob, 1);

			rkit::UniquePtr<rkit::IJobRunner> processJobRunner;
			RKIT_CHECK(rkit::New<AnoxProcessJobRunner>(processJobRunner, factory, resource, loadCompleter, keyPtr));
			RKIT_CHECK(m_jobQueue->CreateJob(nullptr, rkit::JobType::kNormalPriority, std::move(processJobRunner), processJobDepSpan.ToValueISpan()));
		}

		loadFuture = rkit::Future<AnoxResourceRetrieveResult>(std::move(pendingFutureContainer));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxResourceManager::GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		return InternalGetResource<ContentIDKeyedResourceTracker, rkit::data::ContentID, AnoxResourceKeyType::kContentID>(loadFuture, resourceType, cid, &m_contentKeyedResources);
	}

	rkit::Result AnoxResourceManager::GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path)
	{
		return InternalGetResource<CIPathKeyedResourceTracker, rkit::CIPathView, AnoxResourceKeyType::kCIPath>(loadFuture, resourceType, path, &m_pathKeyedResources);
	}

	rkit::Result AnoxResourceManager::GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str)
	{
		return InternalGetResource<StringKeyedResourceTracker, rkit::StringView, AnoxResourceKeyType::kString>(loadFuture, resourceType, str, &m_stringKeyedResources);
	}

	void AnoxResourceManager::UnsyncedUnregisterResource(const AnoxResourceTracker *tracker, bool wasLinked)
	{
		bool isRegistered = false;

		if (m_firstResource == tracker)
		{
			m_firstResource = tracker->m_nextResource;
			isRegistered = true;
		}
		if (m_lastResource == tracker)
		{
			m_lastResource = tracker->m_prevResource;
			isRegistered = true;
		}

		if (isRegistered)
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
	}

	rkit::Result AnoxResourceManagerBase::Create(rkit::UniquePtr<AnoxResourceManagerBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue)
	{
		rkit::UniquePtr<AnoxResourceManager> resLoader;

		RKIT_CHECK(rkit::New<AnoxResourceManager>(resLoader, fileSystem, jobQueue));
		RKIT_CHECK(resLoader->Initialize());

		outResLoader = std::move(resLoader);

		return rkit::ResultCode::kOK;
	}
}
