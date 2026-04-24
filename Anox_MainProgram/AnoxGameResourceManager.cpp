#include "AnoxGameResourceManager.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "AnoxCaptureHarness.h"
#include "AnoxResourceManager.h"

namespace anox::game
{
	class GameResourceManagerImpl final : public rkit::OpaqueImplementation<GameResourceManager>
	{
	public:
		friend class GameResourceManager;

		explicit GameResourceManagerImpl(ICaptureHarness &captureHarness);

		rkit::Result Initialize();

	private:
		struct TypeKeyedResource
		{
			uint32_t m_resType = 0;
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
		static uint32_t ConsumeFreeID(KeyedResourceList<T> &resList);

		rkit::Result AddResourceRequest(uint32_t resType, const rkit::Future<AnoxResourceRetrieveResult> &future);

		ICaptureHarness &m_captureHarness;

		rkit::UniquePtr<rkit::IMutex> m_requestMutex;
		KeyedResourceList<TypeKeyedRequest> m_requests;

		rkit::UniquePtr<rkit::IMutex> m_resourceMutex;
		KeyedResourceList<TypeKeyedResource> m_resources;
	};

	GameResourceManagerImpl::GameResourceManagerImpl(ICaptureHarness &captureHarness)
		: m_captureHarness(captureHarness)
	{
	}

	rkit::Result GameResourceManagerImpl::Initialize()
	{
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->CreateMutex(m_requestMutex));
		RKIT_CHECK(rkit::GetDrivers().m_systemDriver->CreateMutex(m_resourceMutex));

		RKIT_RETURN_OK;
	}


	template<class T>
	rkit::Result GameResourceManagerImpl::ReserveFreeID(KeyedResourceList<T> &resList)
	{
		if (resList.m_numFreeIDs == 0)
		{
			if (resList.m_freeIDs.Count() == 0)
			{
				RKIT_CHECK(resList.m_freeIDs.Append(0));
			}

			if (resList.m_items.Count() > std::numeric_limits<uint32_t>::max() - 1u)
				RKIT_THROW(rkit::ResultCode::kOutOfMemory);

			RKIT_CHECK(resList.m_items.Append(T()));

			resList.m_freeIDs[0] = static_cast<uint32_t>(resList.m_items.Count());
		}

		RKIT_RETURN_OK;
	}

	template<class T>
	uint32_t GameResourceManagerImpl::ConsumeFreeID(KeyedResourceList<T> &resList)
	{
		return resList.m_freeIDs[--resList.m_numFreeIDs];
	}

	rkit::Result GameResourceManagerImpl::AddResourceRequest(uint32_t resType, const rkit::Future<AnoxResourceRetrieveResult> &future)
	{
		rkit::MutexLock lock(*m_requestMutex);

		RKIT_CHECK(ReserveFreeID(m_requests));
		const uint32_t reqID = ConsumeFreeID(m_requests);

		TypeKeyedRequest &req = m_requests.m_items[reqID - 1];
		req.m_resType = resType;
		req.m_future = future;

		RKIT_RETURN_OK;
	}

	rkit::Result GameResourceManager::GetContentIDKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::data::ContentID &contentID)
	{
		if (resourceType == 0)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		GameResourceManagerImpl &impl = Impl();

		rkit::Future<AnoxResourceRetrieveResult> future;
		RKIT_CHECK(impl.m_captureHarness.GetContentIDKeyedResource(future, resourceType, contentID));

		return impl.AddResourceRequest(resourceType, future);
	}

	rkit::Result GameResourceManager::GetCIPathKeyedResource(uint32_t &outReqID, uint32_t resourceType, const rkit::CIPathView &ciPath)
	{
		if (resourceType == 0)
			RKIT_THROW(rkit::ResultCode::kInvalidParameter);

		GameResourceManagerImpl &impl = Impl();

		rkit::Future<AnoxResourceRetrieveResult> future;
		RKIT_CHECK(impl.m_captureHarness.GetCIPathKeyedResource(future, resourceType, ciPath));

		return impl.AddResourceRequest(resourceType, future);
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

				RKIT_CHECK(GameResourceManagerImpl::ReserveFreeID(impl.m_resources));
				resID = GameResourceManagerImpl::ConsumeFreeID(impl.m_resources);

				GameResourceManagerImpl::TypeKeyedResource keyedRes = { };
				keyedRes.m_resource = resultPtr->m_resourceHandle;
				keyedRes.m_resType = req.m_resType;

				// Make the resource done
				impl.m_resources.m_items[resID - 1] = keyedRes;
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

	rkit::Result GameResourceManager::DiscardResource(uint32_t resID)
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

			resDisposeTemp = std::move(res.m_resource);
			res.m_resType = 0;
			res.m_resource.Reset();

			impl.m_resources.m_freeIDs[impl.m_resources.m_numFreeIDs++] = resID;
		}

		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::GameResourceManagerImpl)
