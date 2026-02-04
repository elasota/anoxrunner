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
#include "rkit/Core/Path.h"
#include "rkit/Core/Vector.h"

#include "rkit/Data/ContentID.h"

#include "AnoxBSPModelResource.h"
#include "AnoxEntityDefResource.h"
#include "AnoxFileResource.h"
#include "AnoxMaterialResource.h"
#include "AnoxMDAModelResource.h"
#include "AnoxSpawnDefsResource.h"
#include "AnoxTextureResource.h"

#include <algorithm>

// NOTES ON OWNERSHIP AND REFCOUNT BEHAVIOR:
// Load Job Runner -> AnoxResourceLoadCompletionNotifier -> AnoxResourceLoadFutureContainer -> AnoxResourceTracker

namespace anox
{
	class AnoxResourceManager;
	class AnoxPendingResourceFutureContainer;

	struct AnoxResourceLoaderSynchronizer final : public rkit::RefCounted
	{
		AnoxResourceLoaderSynchronizer(AnoxResourceManager *resManager);

		rkit::Result Init();

		rkit::UniquePtr<rkit::IMutex> m_resourcesMutex;
		AnoxResourceManager *m_resManager;
	};

	struct AnoxResourceTracker : public rkit::RefCountedTracker
	{
		explicit AnoxResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource);
		virtual ~AnoxResourceTracker() {}

		virtual AnoxResourceKeyType GetKeyType() const = 0;

		void RCTrackerZero() override;

		void FailLoading();
		void SucceedLoading();

		rkit::UniquePtr<AnoxResourceBase> m_resource = nullptr;
		uint32_t m_resourceType = 0;

		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> m_pendingFutureContainer;

		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;
		rkit::SimpleObjectAllocation<AnoxResourceTracker> m_self;

		// Only modify these fields under the resource mutex lock
		rkit::RCPtr<rkit::JobSignaler> m_loadCompletionSignaler;
		rkit::RCPtr<rkit::Job> m_loadCompletionJob;
		AnoxResourceTracker *m_prevResource = nullptr;
		AnoxResourceTracker *m_nextResource = nullptr;
	};

	class AnoxResourceLoadCompletionNotifier : public rkit::RefCounted
	{
	public:
		explicit AnoxResourceLoadCompletionNotifier(const rkit::RCPtr<AnoxResourceTracker> &resource);
		~AnoxResourceLoadCompletionNotifier();

		void OnLoadCompleted();

	private:
		rkit::RCPtr<AnoxResourceTracker> m_resource;
	};

	class StringKeyedResourceTracker : public AnoxResourceTracker
	{
	public:
		StringKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource);

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
		ContentIDKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource);

		rkit::Result SetKey(const rkit::data::ContentID &cid);
		const rkit::data::ContentID &GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::data::ContentID m_key;
	};

	class CIPathKeyedResourceTracker : public AnoxResourceTracker
	{
	public:
		CIPathKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource);

		rkit::Result SetKey(const rkit::CIPathView &cid);
		const rkit::CIPathView &GetKey() const;
		AnoxResourceKeyType GetKeyType() const override;

	private:
		rkit::CIPath m_key;
		rkit::CIPathView m_keyView;
	};

	class AnoxCompleteResourceLoadJobRunner final : public rkit::IJobRunner
	{
	public:
		explicit AnoxCompleteResourceLoadJobRunner(const rkit::RCPtr<AnoxResourceLoadCompletionNotifier> &loadCompleter);

		rkit::Result Run() override;

	private:
		rkit::RCPtr<AnoxResourceLoadCompletionNotifier> m_loadCompleter;
	};

	class AnoxResourceManager final : public AnoxResourceManagerBase
	{
	public:
		template<class T>
		class ResourceKey
		{
		public:
			ResourceKey() = default;
			ResourceKey(ResourceKey<T> &&other) = default;
			ResourceKey(const ResourceKey<T> &other) = default;

			explicit ResourceKey(uint32_t resourceType, const T &resourceKey);
			explicit ResourceKey(uint32_t resourceType, T &&resourceKey);

			ResourceKey<T> &operator=(const ResourceKey<T> &other) = default;
			ResourceKey<T> &operator=(ResourceKey<T> &&other) = default;

			uint32_t GetResourceType() const;
			const T &GetResourceKey() const;

			bool operator==(const ResourceKey<T> &other) const;
			bool operator!=(const ResourceKey<T> &other) const;

		private:
			uint32_t m_resourceType = 0;
			T m_resourceKey;
		};

		AnoxResourceManager(AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue);

		rkit::Result Initialize();

		rkit::Result RegisterContentKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::data::ContentID>> &&loader) override;
		rkit::Result RegisterCIPathKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::CIPathView>> &&loader) override;
		rkit::Result RegisterStringKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::StringView>> &&loader) override;

		rkit::Result GetContentIDKeyedResource(rkit::RCPtr<rkit::Job> *loadJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) override;
		rkit::Result GetCIPathKeyedResource(rkit::RCPtr<rkit::Job> *loadJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) override;
		rkit::Result GetStringKeyedResource(rkit::RCPtr<rkit::Job> *loadJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) override;

		rkit::Result EnumerateCIPathKeyedResources(const rkit::CIPathView &basePath, rkit::Vector<rkit::CIPath> &outFiles, rkit::Vector<rkit::CIPath> &outDirectories) const override;

		void SetGraphicsSubsystem(IGraphicsSubsystem *graphicSubsystem) override;

		void UnsyncedUnregisterResource(const AnoxResourceTracker *tracker, bool wasLinked);

	private:
		struct TypeKeyedFactory
		{
			AnoxResourceKeyType m_keyType = AnoxResourceKeyType::kInvalid;
			rkit::RCPtr<AnoxResourceLoaderBase> m_loader;
		};

		rkit::Result InternalRegisterLoader(uint32_t resourceType, AnoxResourceKeyType keyType, rkit::RCPtr<AnoxResourceLoaderBase> &&factory);

		template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType>
		rkit::Result InternalGetResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, const ResourceKey<TKeyViewType> &key, rkit::HashMap<ResourceKey<TKeyViewType>, AnoxResourceTracker *> *resourceMap);

		AnoxGameFileSystemBase *m_fileSystem;
		rkit::IJobQueue *m_jobQueue;

		rkit::HashMap<uint32_t, TypeKeyedFactory> m_loaders;
		rkit::UniquePtr<rkit::IMutex> m_loaderMutex;

		rkit::HashMap<ResourceKey<rkit::CIPathView>, AnoxResourceTracker *> m_pathKeyedResources;
		rkit::HashMap<ResourceKey<rkit::StringView>, AnoxResourceTracker *> m_stringKeyedResources;
		rkit::HashMap<ResourceKey<rkit::data::ContentID>, AnoxResourceTracker *> m_contentKeyedResources;
		rkit::RCPtr<AnoxResourceLoaderSynchronizer> m_sync;

		IGraphicsSubsystem *m_graphicsSubsystem = nullptr;

		AnoxResourceTracker *m_firstResource = nullptr;
		AnoxResourceTracker *m_lastResource = nullptr;
	};
}

namespace rkit
{
	template<class T>
	class Hasher<anox::AnoxResourceManager::ResourceKey<T>>
	{
	public:
		static HashValue_t ComputeHash(HashValue_t baseHash, const anox::AnoxResourceManager::ResourceKey<T> &key);
	};
}

namespace anox
{
	AnoxResourceLoaderSynchronizer::AnoxResourceLoaderSynchronizer(AnoxResourceManager *resManager)
		: m_resManager(resManager)
	{
	}


	rkit::Result AnoxResourceLoaderSynchronizer::Init()
	{
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->CreateMutex(m_resourcesMutex));

		RKIT_RETURN_OK;
	}


	AnoxResourceTracker::AnoxResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: RefCountedTracker(0)
		, m_sync(sync)
		, m_resource(std::move(resource))
		, m_resourceType(resourceType)
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

	void AnoxResourceTracker::FailLoading()
	{
		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> futureContainerRCPtr;
		rkit::RCPtr<rkit::JobSignaler> completionSignaler;

		{
			rkit::MutexLock lock(*m_sync->m_resourcesMutex);

			futureContainerRCPtr = std::move(m_pendingFutureContainer);
			m_pendingFutureContainer.Reset();

			completionSignaler = std::move(m_loadCompletionSignaler);
			m_loadCompletionSignaler.Reset();
			m_loadCompletionJob.Reset();
		}

		futureContainerRCPtr->Fail();

		if (completionSignaler)
			completionSignaler->SignalDone(rkit::ResultCode::kOperationFailed);
	}

	void AnoxResourceTracker::SucceedLoading()
	{
		// Make sure we're keeping hard refs to the resource
		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> futureContainerRCPtr;
		rkit::RCPtr<rkit::JobSignaler> completionSignaler;

		AnoxResourceRetrieveResult result;

		{
			rkit::MutexLock lock(*m_sync->m_resourcesMutex);

			futureContainerRCPtr = std::move(m_pendingFutureContainer);
			m_pendingFutureContainer.Reset();

			completionSignaler = std::move(m_loadCompletionSignaler);
			m_loadCompletionSignaler.Reset();
			m_loadCompletionJob.Reset();

			result.m_resourceHandle = rkit::RCPtr<AnoxResourceBase>(m_resource.Get(), this);
		}

		futureContainerRCPtr->Complete(std::move(result));

		if (completionSignaler)
			completionSignaler->SignalDone(rkit::ResultCode::kOK);
	}

	AnoxResourceLoadCompletionNotifier::AnoxResourceLoadCompletionNotifier(const rkit::RCPtr<AnoxResourceTracker> &resource)
		: m_resource(resource)
	{
	}

	AnoxResourceLoadCompletionNotifier::~AnoxResourceLoadCompletionNotifier()
	{
		if (m_resource)
			m_resource->FailLoading();
	}

	void AnoxResourceLoadCompletionNotifier::OnLoadCompleted()
	{
		m_resource->SucceedLoading();
		m_resource.Reset();
	}

	CIPathKeyedResourceTracker::CIPathKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, resourceType, std::move(resource))
	{
	}

	rkit::Result CIPathKeyedResourceTracker::SetKey(const rkit::CIPathView &path)
	{
		RKIT_CHECK(m_key.Set(path));
		m_keyView = m_key;

		RKIT_RETURN_OK;
	}

	const rkit::CIPathView &CIPathKeyedResourceTracker::GetKey() const
	{
		return m_keyView;
	}

	AnoxResourceKeyType CIPathKeyedResourceTracker::GetKeyType() const
	{
		return AnoxResourceKeyType::kCIPath;
	}

	AnoxCompleteResourceLoadJobRunner::AnoxCompleteResourceLoadJobRunner(const rkit::RCPtr<AnoxResourceLoadCompletionNotifier> &loadCompleter)
		: m_loadCompleter(loadCompleter)
	{
	}

	rkit::Result AnoxCompleteResourceLoadJobRunner::Run()
	{
		m_loadCompleter->OnLoadCompleted();
		RKIT_RETURN_OK;
	}

	StringKeyedResourceTracker::StringKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, resourceType, std::move(resource))
	{
	}

	rkit::Result StringKeyedResourceTracker::SetKey(const rkit::StringView &str)
	{
		RKIT_CHECK(m_key.Set(str));
		m_keyView = m_key;

		RKIT_RETURN_OK;
	}

	const rkit::StringView &StringKeyedResourceTracker::GetKey() const
	{
		return m_keyView;
	}

	AnoxResourceKeyType StringKeyedResourceTracker::GetKeyType() const
	{
		return AnoxResourceKeyType::kString;
	}

	ContentIDKeyedResourceTracker::ContentIDKeyedResourceTracker(const rkit::RCPtr<AnoxResourceLoaderSynchronizer> &sync, uint32_t resourceType, rkit::UniquePtr<AnoxResourceBase> &&resource)
		: AnoxResourceTracker(sync, resourceType, std::move(resource))
	{
	}

	rkit::Result ContentIDKeyedResourceTracker::SetKey(const rkit::data::ContentID &cid)
	{
		m_key = cid;
		RKIT_RETURN_OK;
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
		RKIT_CHECK(sysDriver->CreateMutex(m_loaderMutex));

		RKIT_CHECK(m_sync->Init());

		{
			rkit::RCPtr<AnoxFileResourceLoaderBase> fileLoaderFactory;
			RKIT_CHECK(AnoxFileResourceLoaderBase::Create(fileLoaderFactory));

			RKIT_CHECK(RegisterCIPathKeyedLoader(resloaders::kRawFileResourceTypeCode, std::move(fileLoaderFactory)));
		}

		{
			rkit::RCPtr<AnoxBSPModelResourceLoaderBase> bspLoaderFactory;
			RKIT_CHECK(AnoxBSPModelResourceLoaderBase::Create(bspLoaderFactory));

			RKIT_CHECK(RegisterCIPathKeyedLoader(resloaders::kBSPModelResourceTypeCode, std::move(bspLoaderFactory)));
		}

		{
			rkit::RCPtr<AnoxSpawnDefsResourceLoaderBase> spawnDefsLoaderFactory;
			RKIT_CHECK(AnoxSpawnDefsResourceLoaderBase::Create(spawnDefsLoaderFactory));

			RKIT_CHECK(RegisterCIPathKeyedLoader(resloaders::kSpawnDefsResourceTypeCode, std::move(spawnDefsLoaderFactory)));
		}

		{
			rkit::RCPtr<AnoxTextureResourceLoaderBase> texLoader;
			RKIT_CHECK(AnoxTextureResourceLoaderBase::Create(texLoader));

			RKIT_CHECK(RegisterContentKeyedLoader(resloaders::kTextureResourceTypeCode, std::move(texLoader)));
		}

		{
			rkit::RCPtr<AnoxMaterialResourceLoaderBase> worldMaterialLoader;
			RKIT_CHECK(AnoxMaterialResourceLoaderBase::Create(worldMaterialLoader, data::MaterialResourceType::kWorld));

			RKIT_CHECK(RegisterContentKeyedLoader(resloaders::kWorldMaterialTypeCode, std::move(worldMaterialLoader)));
		}

		{
			rkit::RCPtr<AnoxMaterialResourceLoaderBase> modelMaterialLoader;
			RKIT_CHECK(AnoxMaterialResourceLoaderBase::Create(modelMaterialLoader, data::MaterialResourceType::kModel));

			RKIT_CHECK(RegisterContentKeyedLoader(resloaders::kModelMaterialTypeCode, std::move(modelMaterialLoader)));
		}

		{
			rkit::RCPtr<AnoxEntityDefResourceLoaderBase> edefLoader;
			RKIT_CHECK(AnoxEntityDefResourceLoaderBase::Create(edefLoader));

			RKIT_CHECK(RegisterContentKeyedLoader(resloaders::kEntityDefTypeCode, std::move(edefLoader)));
		}

		{
			rkit::RCPtr<AnoxMDAModelResourceLoaderBase> mdaLoader;
			RKIT_CHECK(AnoxMDAModelResourceLoaderBase::Create(mdaLoader));

			RKIT_CHECK(RegisterContentKeyedLoader(resloaders::kMDAModelResourceTypeCode, std::move(mdaLoader)));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxResourceManager::RegisterContentKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::data::ContentID>> &&loader)
	{
		return InternalRegisterLoader(resourceType, AnoxResourceKeyType::kContentID, std::move(loader));
	}

	rkit::Result AnoxResourceManager::RegisterCIPathKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::CIPathView>> &&loader)
	{
		return InternalRegisterLoader(resourceType, AnoxResourceKeyType::kCIPath, std::move(loader));
	}

	rkit::Result AnoxResourceManager::RegisterStringKeyedLoader(uint32_t resourceType, rkit::RCPtr<AnoxKeyedResourceLoader<rkit::StringView>> &&loader)
	{
		return InternalRegisterLoader(resourceType, AnoxResourceKeyType::kString, std::move(loader));
	}

	rkit::Result AnoxResourceManager::InternalRegisterLoader(uint32_t resourceType, AnoxResourceKeyType keyType, rkit::RCPtr<AnoxResourceLoaderBase> &&loader)
	{
		rkit::MutexLock lock(*m_loaderMutex);

		TypeKeyedFactory keyedFactory;
		keyedFactory.m_keyType = keyType;
		keyedFactory.m_loader = std::move(loader);

		RKIT_CHECK(m_loaders.Set(resourceType, std::move(keyedFactory)));

		RKIT_RETURN_OK;
	}

	template<class TKeyedTracker, class TKeyViewType, AnoxResourceKeyType TKeyType>
	rkit::Result AnoxResourceManager::InternalGetResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, const ResourceKey<TKeyViewType> &key, rkit::HashMap<ResourceKey<TKeyViewType>, AnoxResourceTracker *> *resourceMap)
	{
		loadFuture.Reset();

		typedef typename rkit::HashMap<ResourceKey<TKeyViewType>, AnoxResourceTracker *>::ConstIterator_t MapConstIterator_t;
		typedef typename rkit::HashMap<ResourceKey<TKeyViewType>, AnoxResourceTracker *>::Iterator_t MapIterator_t;

		// The loadCompleter and resourceRCPtr must be before the lock so that
		// if a failure occurs in this function, they will be destroyed outside
		// of the lock, since their destruction will lock the resource mutex.
		rkit::RCPtr<AnoxResourceBase> resourceRCPtr;
		rkit::RCPtr<AnoxResourceLoadCompletionNotifier> loadCompleter;

		rkit::HashValue_t hash = rkit::Hasher<ResourceKey<TKeyViewType>>::ComputeHash(0, key);

		rkit::MutexLock resLock(*m_sync->m_resourcesMutex);

		MapIterator_t it = resourceMap->FindPrehashed(hash, key);
		if (it != resourceMap->end())
		{
			// Resource is registered
			AnoxResourceTracker *tracker = it.Value();

			if (tracker->m_pendingFutureContainer.IsValid())
			{
				// Resource is not loaded
				loadFuture = rkit::Future<AnoxResourceRetrieveResult>(tracker->m_pendingFutureContainer);

				if (outJob)
				{
					// We don't create the signaler/completion job normally, only if there are multiple requests for
					// the same resource
					if (!tracker->m_loadCompletionSignaler)
					{
						RKIT_CHECK(m_jobQueue->CreateSignaledJob(tracker->m_loadCompletionSignaler, tracker->m_loadCompletionJob));
					}

					*outJob = tracker->m_loadCompletionJob;
				}

				resLock.Unlock();

				RKIT_RETURN_OK;
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
				RKIT_RETURN_OK;
			}
		}

		// Resource is not registered
		rkit::RCPtr<AnoxResourceLoaderBase> loader;
		{
			rkit::MutexLock factoryLock(*m_loaderMutex);
			rkit::HashMap<uint32_t, TypeKeyedFactory>::ConstIterator_t factoryIt = m_loaders.Find(key.GetResourceType());
			if (factoryIt == m_loaders.end() || factoryIt.Value().m_keyType != TKeyType)
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			loader = factoryIt.Value().m_loader;
		}

		// Create the resource
		rkit::UniquePtr<AnoxResourceBase> resourceUPtr;
		RKIT_CHECK(loader->BaseCreateResourceObject(resourceUPtr));

		AnoxResourceBase *resource = resourceUPtr.Get();

		// Create the tracker and form a RC ptr from it immediately.
		// We have to do this because the unregistration happens in AnoxResourceTracker::RCTrackerZero,
		// not the AnoxResourceTracker destructor.  However, unregistration ONLY happens if the resource
		// has been linked.
		rkit::UniquePtr<TKeyedTracker> keyedTrackerUPtr;
		RKIT_CHECK(rkit::New<TKeyedTracker>(keyedTrackerUPtr, m_sync, key.GetResourceType(), std::move(resourceUPtr)));

		RKIT_CHECK(keyedTrackerUPtr->SetKey(key.GetResourceKey()));

		TKeyedTracker *keyedTracker = keyedTrackerUPtr.Get();
		AnoxResourceTracker *tracker = keyedTracker;
		tracker->m_self = keyedTrackerUPtr.Detach();

		resourceRCPtr = rkit::RCPtr<AnoxResourceBase>(resource, tracker);

		// Now that resourceRCPtr is set, failure will cause the mutex to be unlocked, followed by
		// re-lock and unregistration of the resource by resourceRCPtr
		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> pendingFutureContainer;
		RKIT_CHECK(rkit::New<rkit::FutureContainer<AnoxResourceRetrieveResult>>(pendingFutureContainer));

		tracker->m_pendingFutureContainer = pendingFutureContainer;

		// Create the completion notifier
		rkit::RCPtr<AnoxResourceTracker> trackerRCPtr = rkit::RCPtr<AnoxResourceTracker>(tracker, tracker);

		RKIT_CHECK(rkit::New<AnoxResourceLoadCompletionNotifier>(loadCompleter, trackerRCPtr));

		rkit::RCPtr<rkit::FutureContainer<AnoxResourceRetrieveResult>> recastContainer;

		// Add to the resource map and link.  Both operations must be done together atomically,
		// since UnsyncedUnregisterResource will check if the resource is linked to determine
		// if it needs to be removed from the resource map.
		RKIT_CHECK(resourceMap->SetPrehashed(hash, ResourceKey<TKeyViewType>(key.GetResourceType(), keyedTracker->GetKey()), tracker));

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

		// Add the load job
		rkit::RCPtr<rkit::Job> loadJob;
		const void *keyPtr = &keyedTracker->GetKey();

		AnoxResourceLoaderSystems systems =
		{
			this,
			m_fileSystem,
			m_graphicsSubsystem
		};

		RKIT_CHECK(loader->BaseCreateLoadJob(resourceRCPtr, systems, keyPtr, loadJob));

		{
			rkit::ConstSpan<rkit::RCPtr<rkit::Job>> completeJobDepSpan;
			if (loadJob.IsValid())
				completeJobDepSpan = rkit::Span<rkit::RCPtr<rkit::Job>>(&loadJob, 1);

			rkit::UniquePtr<rkit::IJobRunner> completeJobRunner;
			RKIT_CHECK(rkit::New<AnoxCompleteResourceLoadJobRunner>(completeJobRunner, loadCompleter));
			RKIT_CHECK(m_jobQueue->CreateJob(outJob, rkit::JobType::kNormalPriority, std::move(completeJobRunner), completeJobDepSpan));
		}

		loadFuture = rkit::Future<AnoxResourceRetrieveResult>(std::move(pendingFutureContainer));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxResourceManager::GetContentIDKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		return InternalGetResource<ContentIDKeyedResourceTracker, rkit::data::ContentID, AnoxResourceKeyType::kContentID>(outJob, loadFuture, ResourceKey<rkit::data::ContentID>(resourceType, cid), &m_contentKeyedResources);
	}

	rkit::Result AnoxResourceManager::GetCIPathKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path)
	{
		return InternalGetResource<CIPathKeyedResourceTracker, rkit::CIPathView, AnoxResourceKeyType::kCIPath>(outJob, loadFuture, ResourceKey<rkit::CIPathView>(resourceType, path), &m_pathKeyedResources);
	}

	rkit::Result AnoxResourceManager::GetStringKeyedResource(rkit::RCPtr<rkit::Job> *outJob, rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str)
	{
		return InternalGetResource<StringKeyedResourceTracker, rkit::StringView, AnoxResourceKeyType::kString>(outJob, loadFuture, ResourceKey<rkit::StringView>(resourceType, str), &m_stringKeyedResources);
	}

	rkit::Result AnoxResourceManager::EnumerateCIPathKeyedResources(const rkit::CIPathView &basePath, rkit::Vector<rkit::CIPath> &outFiles, rkit::Vector<rkit::CIPath> &outDirectories) const
	{
		RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
	}

	void AnoxResourceManager::SetGraphicsSubsystem(IGraphicsSubsystem *graphicsSubsystem)
	{
		m_graphicsSubsystem = graphicsSubsystem;
	}

	void AnoxResourceManager::UnsyncedUnregisterResource(const AnoxResourceTracker *tracker, bool wasLinked)
	{
		bool isRegistered = wasLinked;

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
				m_pathKeyedResources.Remove(ResourceKey<rkit::CIPathView>(tracker->m_resourceType, static_cast<const CIPathKeyedResourceTracker *>(tracker)->GetKey()));
				break;

			case AnoxResourceKeyType::kContentID:
				m_contentKeyedResources.Remove(ResourceKey<rkit::data::ContentID>(tracker->m_resourceType, static_cast<const ContentIDKeyedResourceTracker *>(tracker)->GetKey()));
				break;

			case AnoxResourceKeyType::kString:
				m_stringKeyedResources.Remove(ResourceKey<rkit::StringView>(tracker->m_resourceType, static_cast<const StringKeyedResourceTracker *>(tracker)->GetKey()));
				break;
			default:
				break;
			}
		}
	}


	template<class T>
	AnoxResourceManager::ResourceKey<T>::ResourceKey(uint32_t resourceType, const T &resourceKey)
		: m_resourceType(resourceType)
		, m_resourceKey(resourceKey)
	{
	}

	template<class T>
	AnoxResourceManager::ResourceKey<T>::ResourceKey(uint32_t resourceType, T &&resourceKey)
		: m_resourceType(resourceType)
		, m_resourceKey(std::move(resourceKey))
	{
	}

	template<class T>
	uint32_t AnoxResourceManager::ResourceKey<T>::GetResourceType() const
	{
		return m_resourceType;
	}

	template<class T>
	const T &AnoxResourceManager::ResourceKey<T>::GetResourceKey() const
	{
		return m_resourceKey;
	}

	template<class T>
	bool AnoxResourceManager::ResourceKey<T>::operator==(const ResourceKey<T> &other) const
	{
		return m_resourceType == other.m_resourceType && m_resourceKey == other.m_resourceKey;
	}

	template<class T>
	bool AnoxResourceManager::ResourceKey<T>::operator!=(const ResourceKey<T> &other) const
	{
		return !((*this) == other);
	}

	rkit::Result AnoxResourceManagerBase::Create(rkit::UniquePtr<AnoxResourceManagerBase> &outResLoader, AnoxGameFileSystemBase *fileSystem, rkit::IJobQueue *jobQueue)
	{
		rkit::UniquePtr<AnoxResourceManager> resLoader;

		RKIT_CHECK(rkit::New<AnoxResourceManager>(resLoader, fileSystem, jobQueue));
		RKIT_CHECK(resLoader->Initialize());

		outResLoader = std::move(resLoader);

		RKIT_RETURN_OK;
	}
}

namespace rkit
{
	template<class T>
	HashValue_t Hasher<anox::AnoxResourceManager::ResourceKey<T>>::ComputeHash(HashValue_t baseHash, const anox::AnoxResourceManager::ResourceKey<T> &key)
	{
		HashValue_t hash = baseHash;
		hash = Hasher<uint32_t>::ComputeHash(hash, key.GetResourceType());
		hash = Hasher<T>::ComputeHash(hash, key.GetResourceKey());
		return hash;
	}
}
