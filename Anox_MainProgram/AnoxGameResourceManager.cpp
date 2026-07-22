#include "AnoxGameResourceManager.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "anox/Data/ResourceTypeCodes.h"

#include "AnoxCaptureHarness.h"
#include "AnoxFileResource.h"
#include "AnoxResourceManager.h"

namespace anox::game
{
	class GameResourceManagerImpl final : public rkit::OpaqueImplementation<GameResourceManager>
	{
	public:
		friend class GameResourceManager;

		rkit::Result Initialize();

		rkit::Result AcquireResource(rkit::RCPtr<AnoxResourceBase> &outResourcePtr, uint32_t &outResourceType, uint32_t resID) const;

	private:
		struct TypeKeyedResource
		{
			uint32_t m_resType = 0;
			uint32_t m_refCount = 0;
			rkit::RCPtr<AnoxResourceBase> m_resource;
		};

		struct TypeKeyedRequest
		{
			uint32_t m_resType = 0;
			rkit::Future<AnoxResourceRetrieveResult> m_future;
		};

		template<class T>
		struct KeyedResourceList
		{
			// The free list capacity is always as large as the item list
			rkit::Vector<uint32_t> m_freeIDs;
			size_t m_numFreeIDs = 0;

			rkit::Vector<T> m_items;
		};

		template<class T>
		static rkit::Result ReserveFreeID(KeyedResourceList<T> &resList);

		template<class T>
		static uint32_t PeekFreeID(KeyedResourceList<T> &resList);

		template<class T>
		static void DiscardFreeID(KeyedResourceList<T> &resList);

		rkit::Result AddResourceRequest(uint32_t &outReqID, uint32_t resType, const rkit::Future<AnoxResourceRetrieveResult> &future);

		ICaptureHarness *m_captureHarness = nullptr;

		rkit::UniquePtr<rkit::IMutex> m_requestMutex;
		KeyedResourceList<TypeKeyedRequest> m_requests;

		rkit::UniquePtr<rkit::IMutex> m_resourceMutex;
		KeyedResourceList<TypeKeyedResource> m_resources;
		rkit::HashMap<const AnoxResourceBase *, uint32_t> m_resourcesMap;
	};

	rkit::Result GameResourceManagerImpl::Initialize()
	{
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->CreateMutex(m_requestMutex));
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->CreateMutex(m_resourceMutex));

		RKIT_RETURN_OK;
	}

	rkit::Result GameResourceManagerImpl::AcquireResource(rkit::RCPtr<AnoxResourceBase> &outResourcePtr, uint32_t &outResourceType, uint32_t resID) const
	{
		if (resID == 0)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		rkit::MutexLock lock(*m_resourceMutex);
		if (resID > m_resources.m_items.Count())
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		const TypeKeyedResource &res = m_resources.m_items[resID - 1];
		if (res.m_resType == 0)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		outResourcePtr = res.m_resource;
		outResourceType = res.m_resType;

		RKIT_RETURN_OK;
	}

	template<class T>
	rkit::Result GameResourceManagerImpl::ReserveFreeID(KeyedResourceList<T> &resList)
	{
		if (resList.m_numFreeIDs == 0)
		{
			if (resList.m_freeIDs.Count() <= resList.m_items.Count())
			{
				RKIT_CHECK(resList.m_freeIDs.Append(0));
			}

			if (resList.m_items.Count() > std::numeric_limits<uint32_t>::max() - 1u)
				RKIT_THROW(rkit::ResultCode::kOutOfMemory);

			RKIT_CHECK(resList.m_items.Append(T()));

			resList.m_freeIDs[0] = static_cast<uint32_t>(resList.m_items.Count());
			resList.m_numFreeIDs = 1;
		}

		RKIT_RETURN_OK;
	}

	template<class T>
	uint32_t GameResourceManagerImpl::PeekFreeID(KeyedResourceList<T> &resList)
	{
		return resList.m_freeIDs[resList.m_numFreeIDs - 1];
	}

	template<class T>
	void GameResourceManagerImpl::DiscardFreeID(KeyedResourceList<T> &resList)
	{
		resList.m_numFreeIDs--;
	}

	rkit::Result GameResourceManagerImpl::AddResourceRequest(uint32_t &outReqID, uint32_t resType, const rkit::Future<AnoxResourceRetrieveResult> &future)
	{
		rkit::MutexLock lock(*m_requestMutex);

		RKIT_CHECK(ReserveFreeID(m_requests));
		const uint32_t reqID = PeekFreeID(m_requests);
		DiscardFreeID(m_requests);

		TypeKeyedRequest &req = m_requests.m_items[reqID - 1];
		req.m_resType = resType;
		req.m_future = future;

		outReqID = reqID;

		RKIT_RETURN_OK;
	}

	void GameResourceManager::SetCaptureHarness(ICaptureHarness *captureHarness)
	{
		Impl().m_captureHarness = captureHarness;
	}

	rkit::Result GameResourceManager::GetContentIDKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::data::ContentID &contentID)
	{
		if (resourceType == 0)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		GameResourceManagerImpl &impl = Impl();

		rkit::Future<AnoxResourceRetrieveResult> future;
		RKIT_CHECK(impl.m_captureHarness->GetContentIDKeyedResource(future, resourceType, contentID));

		return impl.AddResourceRequest(outReqID, resourceType, future);
	}

	rkit::Result GameResourceManager::GetCIPathKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::CIPathView &ciPath)
	{
		if (resourceType == 0)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		GameResourceManagerImpl &impl = Impl();

		rkit::Future<AnoxResourceRetrieveResult> future;
		RKIT_CHECK(impl.m_captureHarness->GetCIPathKeyedResource(future, resourceType, ciPath));

		return impl.AddResourceRequest(outReqID, resourceType, future);
	}

	rkit::Result GameResourceManager::TryFinishLoadingResourceRequest(rkit::Optional<uint32_t> &outResID, uint32_t reqID)
	{
		GameResourceManagerImpl &impl = Impl();

		uint32_t resID = 0;
		rkit::Future<AnoxResourceRetrieveResult> reqDisposeTemp;

		{
			rkit::MutexLock lock(*impl.m_requestMutex);

			if (reqID == 0 || reqID > impl.m_requests.m_items.Count())
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			GameResourceManagerImpl::TypeKeyedRequest &req = impl.m_requests.m_items[reqID - 1];

			if (req.m_resType == 0)
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			const AnoxResourceRetrieveResult *resultPtr = req.m_future.TryGetResult();
			if (!resultPtr)
			{
				outResID = rkit::Optional<uint32_t>();
				RKIT_RETURN_OK;
			}

			{
				rkit::MutexLock lock(*impl.m_resourceMutex);

				const anox::AnoxResourceBase *resourcePtr = resultPtr->m_resourceHandle.Get();
				const rkit::HashValue_t resourcePtrHash = rkit::Hasher<const anox::AnoxResourceBase *>::ComputeHash(0, resourcePtr);

				bool isExistingResource = false;

				{
					rkit::HashMap<const AnoxResourceBase *, uint32_t>::ConstIterator_t it = impl.m_resourcesMap.FindPrehashed(resourcePtrHash, resourcePtr);
					if (it != impl.m_resourcesMap.end())
					{
						resID = it.Value();

						uint32_t &refCountRef = impl.m_resources.m_items[resID - 1].m_refCount;
						if (refCountRef == std::numeric_limits<uint32_t>::max())
							RKIT_THROW(rkit::ResultCode::kIntegerOverflow);

						refCountRef++;
						isExistingResource = true;
					}
				}

				if (!isExistingResource)
				{
					RKIT_CHECK(GameResourceManagerImpl::ReserveFreeID(impl.m_resources));

					// Peek the free ID in case the hash map set fails
					resID = GameResourceManagerImpl::PeekFreeID(impl.m_resources);

					RKIT_CHECK(impl.m_resourcesMap.SetPrehashed(resourcePtrHash, resourcePtr, resID));

					GameResourceManagerImpl::DiscardFreeID(impl.m_resources);

					GameResourceManagerImpl::TypeKeyedResource keyedRes = { };
					keyedRes.m_resource = resultPtr->m_resourceHandle;
					keyedRes.m_resType = req.m_resType;
					keyedRes.m_refCount = 1;

					// Make the resource done and add it
					impl.m_resources.m_items[resID - 1] = keyedRes;
				}
			}

			// Clear the request
			reqDisposeTemp = std::move(req.m_future);

			req.m_future.Reset();
			req.m_resType = 0;
			impl.m_requests.m_freeIDs[impl.m_requests.m_numFreeIDs++] = reqID;
		}

		outResID = resID;

		RKIT_RETURN_OK;
	}

	rkit::Result GameResourceManager::DiscardRequest(uint32_t reqID)
	{
		rkit::Future<AnoxResourceRetrieveResult> reqDisposeTemp;

		GameResourceManagerImpl &impl = Impl();

		{
			rkit::MutexLock lock(*impl.m_requestMutex);

			if (reqID == 0 || reqID > impl.m_requests.m_items.Count())
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			GameResourceManagerImpl::TypeKeyedRequest &req = impl.m_requests.m_items[reqID - 1];

			if (req.m_resType == 0)
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			reqDisposeTemp = std::move(req.m_future);
			req.m_resType = 0;
			req.m_future.Reset();

			impl.m_requests.m_freeIDs[impl.m_requests.m_numFreeIDs++] = reqID;
		}

		RKIT_RETURN_OK;
	}

	rkit::Result GameResourceManager::DecRefResource(uint32_t resID)
	{
		rkit::RCPtr<AnoxResourceBase> resDisposeTemp;

		GameResourceManagerImpl &impl = Impl();

		{
			rkit::MutexLock lock(*impl.m_requestMutex);

			if (resID == 0 || resID > impl.m_resources.m_items.Count())
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			GameResourceManagerImpl::TypeKeyedResource &res = impl.m_resources.m_items[resID - 1];

			if (res.m_resType == 0)
				RKIT_THROW(rkit::ResultCode::kInvalidParameter);

			res.m_refCount--;
			if (res.m_refCount == 0)
			{
				const anox::AnoxResourceBase *resPtr = res.m_resource.Get();
				impl.m_resourcesMap.Remove(resPtr);

				resDisposeTemp = std::move(res.m_resource);
				res.m_resType = 0;
				res.m_resource.Reset();

				impl.m_resources.m_freeIDs[impl.m_resources.m_numFreeIDs++] = resID;
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result GameResourceManager::GetFileResourceContents(rkit::RCPtr<AnoxResourceBase> &outKeepAlive, rkit::Span<const uint8_t> &outBytes, uint32_t resID) const
	{
		const GameResourceManagerImpl &impl = Impl();

		rkit::RCPtr<AnoxResourceBase> res;
		uint32_t resType = 0;
		RKIT_CHECK(impl.AcquireResource(res, resType, resID));

		if (resType != resloaders::kCIPathRawFileResourceTypeCode && resType != resloaders::kContentIDRawFileResourceTypeCode)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		outBytes = static_cast<AnoxFileResourceBase *>(res.Get())->GetContents();
		outKeepAlive = std::move(res);

		RKIT_RETURN_OK;
	}

	rkit::Result GameResourceManager::Create(rkit::UniquePtr<GameResourceManager> &outResManager)
	{
		rkit::UniquePtr<GameResourceManager> resManager;
		RKIT_CHECK(rkit::New<GameResourceManager>(resManager));
		RKIT_CHECK(resManager->Impl().Initialize());

		outResManager = std::move(resManager);

		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::GameResourceManagerImpl)
