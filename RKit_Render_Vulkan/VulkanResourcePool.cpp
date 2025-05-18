#include "VulkanResourcePool.h"

#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "IncludeVulkan.h"

namespace rkit { namespace render { namespace vulkan
{
	template<class T>
	class ResourcePool final : public IResourcePool<T>
	{
	public:
		explicit ResourcePool(const IPooledResourceFactory<T> &factory);
		~ResourcePool();

		Result Initialize();

		Result AcquireResource(T &outResource, size_t &outResourceIndex) override;
		void RetireResource(size_t resourceIndex) override;

	private:
		struct InterleavedListElement
		{
			T m_resource;
			size_t m_freeIndex;
		};

		const IPooledResourceFactory<T> &m_factory;

		// These are interleaved so we never have to append in RetireResource
		Vector<InterleavedListElement> m_elements;

		size_t m_numConstructedResources = 0;
		size_t m_numFreeResources = 0;
	};

	template<class T>
	class MutexProtectedResourcePool final : public IResourcePool<T>
	{
	public:
		explicit MutexProtectedResourcePool(const IPooledResourceFactory<T> &factory);
		~MutexProtectedResourcePool();

		Result Initialize();

		Result AcquireResource(T &outResource, size_t &outResourceIndex) override;
		void RetireResource(size_t resourceIndex) override;

	private:
		ResourcePool<T> m_pool;
		UniquePtr<IMutex> m_mutex;
	};

	template<class T>
	ResourcePool<T>::ResourcePool(const IPooledResourceFactory<T> &factory)
		: m_factory(factory)
	{
	}

	template<class T>
	ResourcePool<T>::~ResourcePool()
	{
		const size_t numResources = m_numConstructedResources;

		InterleavedListElement *elements = m_elements.GetBuffer();
		for (size_t i = 0; i < numResources; i++)
			m_factory.DestructResource(elements[i].m_resource);
	}

	template<class T>
	Result ResourcePool<T>::Initialize()
	{
		return ResultCode::kOK;
	}

	template<class T>
	Result ResourcePool<T>::AcquireResource(T &outResource, size_t &outResourceIndex)
	{
		if (m_numFreeResources > 0)
		{
			InterleavedListElement *elements = m_elements.GetBuffer();

			size_t freeResIndex = elements[0].m_freeIndex;

			T &resRef = elements[freeResIndex].m_resource;
			RKIT_CHECK(m_factory.UnretireResource(resRef));

			elements[0].m_freeIndex = elements[--m_numFreeResources].m_freeIndex;

			outResource = resRef;
			outResourceIndex = freeResIndex;

			return ResultCode::kOK;
		}
		else
		{
			const size_t resIndex = m_numConstructedResources;
			if (resIndex == m_elements.Count())
			{
				InterleavedListElement element;

				element.m_freeIndex = resIndex;
				m_factory.EmptyInitResource(element.m_resource);
				RKIT_CHECK(m_elements.Append(element));
			}

			T &newResource = m_elements[resIndex].m_resource;
			RKIT_CHECK(m_factory.InitResource(newResource));

			m_numConstructedResources = resIndex + 1;

			outResource = newResource;
			outResourceIndex = resIndex;

			return ResultCode::kOK;
		}
	}

	template<class T>
	void ResourcePool<T>::RetireResource(size_t resourceIndex)
	{
		m_elements[m_numFreeResources++].m_freeIndex = resourceIndex;
		m_factory.RetireResource(m_elements[resourceIndex].m_resource);
	}

	template<class T>
	MutexProtectedResourcePool<T>::MutexProtectedResourcePool(const IPooledResourceFactory<T> &factory)
		: m_pool(factory)
	{
	}

	template<class T>
	MutexProtectedResourcePool<T>::~MutexProtectedResourcePool()
	{
	}

	template<class T>
	Result MutexProtectedResourcePool<T>::Initialize()
	{
		RKIT_CHECK(GetDrivers().m_systemDriver->CreateMutex(m_mutex));
		RKIT_CHECK(m_pool.Initialize());

		return ResultCode::kOK;
	}

	template<class T>
	Result MutexProtectedResourcePool<T>::AcquireResource(T &outResource, size_t &outResourceIndex)
	{
		MutexLock lock(*m_mutex);
		return m_pool.AcquireResource(outResource, outResourceIndex);
	}

	template<class T>
	void MutexProtectedResourcePool<T>::RetireResource(size_t resourceIndex)
	{
		MutexLock lock(*m_mutex);
		m_pool.RetireResource(resourceIndex);
	}

	template<class T>
	Result CreateResourcePool(UniquePtr<IResourcePool<T>> &outPool, const IPooledResourceFactory<T> &factory, bool mutexProtected)
	{
		if (mutexProtected)
		{
			UniquePtr<MutexProtectedResourcePool<T>> pool;
			RKIT_CHECK(New<MutexProtectedResourcePool<T>>(pool, factory));
			RKIT_CHECK(pool->Initialize());
			outPool = std::move(pool);
		}
		else
		{
			UniquePtr<ResourcePool<T>> pool;
			RKIT_CHECK(New<ResourcePool<T>>(pool, factory));
			RKIT_CHECK(pool->Initialize());
			outPool = std::move(pool);
		}

		return ResultCode::kOK;
	}

	template Result CreateResourcePool<VkSemaphore>(UniquePtr<IResourcePool<VkSemaphore>> &outPool, const IPooledResourceFactory<VkSemaphore> &factory, bool mutexProtected);
	template Result CreateResourcePool<VkFence>(UniquePtr<IResourcePool<VkFence>> &outPool, const IPooledResourceFactory<VkFence> &factory, bool mutexProtected);
} } } // rkit::render::vulkan
