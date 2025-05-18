#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/NoCopy.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace rkit { namespace render { namespace vulkan
{
	template<class T>
	struct IPooledResourceFactory
	{
		virtual void EmptyInitResource(T &outResource) const = 0;
		virtual void DestructResource(T &resource) const = 0;

		virtual Result InitResource(T &resource) const = 0;
		virtual void RetireResource(T &resource) const = 0;
		virtual Result UnretireResource(T &resource) const = 0;
	};

	template<class T>
	struct IResourcePool
	{
		virtual ~IResourcePool() {}

		virtual Result AcquireResource(T &outResource, size_t &outResourceIndex) = 0;
		virtual void RetireResource(size_t resourceIndex) = 0;
	};

	template<class T>
	class UniqueResourceRef : public NoCopy
	{
	public:
		UniqueResourceRef();
		UniqueResourceRef(UniqueResourceRef &&other);
		~UniqueResourceRef();

		UniqueResourceRef<T> &operator=(UniqueResourceRef<T> &&other);

		Result AcquireFrom(IResourcePool<T> &pool);
		void Release(IResourcePool<T> *&outPool, T &outResource, size_t &outIndex);
		void Reset();

		bool IsValid() const;

		const T &GetResource() const;
		IResourcePool<T> *GetPool() const;

	private:
		IResourcePool<T> *m_pool;
		T m_resource;
		size_t m_resIndex;
	};

	template<class T>
	Result CreateResourcePool(UniquePtr<IResourcePool<T>> &outPool, const IPooledResourceFactory<T> &factory, bool mutexProtected);
} } } // rkit::render::vulkan

#include "rkit/Core/Result.h"

#include <utility>

namespace rkit { namespace render { namespace vulkan
{
	template<class T>
	UniqueResourceRef<T>::UniqueResourceRef()
		: m_pool(nullptr)
		, m_resource()
		, m_resIndex(0)
	{
	}

	template<class T>
	UniqueResourceRef<T>::UniqueResourceRef(UniqueResourceRef &&other)
		: m_pool(other.m_pool)
		, m_resource(std::move(other.m_resource))
		, m_resIndex(other.m_resIndex)
	{
		other.m_pool = nullptr;
	}

	template<class T>
	UniqueResourceRef<T>::~UniqueResourceRef()
	{
		if (m_pool)
			m_pool->RetireResource(m_resIndex);
	}

	template<class T>
	UniqueResourceRef<T> &UniqueResourceRef<T>::operator=(UniqueResourceRef<T> &&other)
	{
		IResourcePool<T> *oldPool = m_pool;
		T oldResource = std::move(m_resource);
		size_t oldResIndex = m_resIndex;

		m_pool = other.m_pool;
		m_resource = std::move(other.m_resource);
		m_resIndex = other.m_resIndex;

		other.m_pool = nullptr;

		if (oldPool)
			oldPool->RetireResource(oldResIndex);

		return *this;
	}

	template<class T>
	Result UniqueResourceRef<T>::AcquireFrom(IResourcePool<T> &pool)
	{
		IResourcePool<T> *oldPool = m_pool;
		T oldResource = std::move(m_resource);
		size_t oldResIndex = m_resIndex;

		T newResource;
		size_t newResIndex = 0;
		RKIT_CHECK(pool.AcquireResource(newResource, newResIndex));

		m_resIndex = newResIndex;
		m_resource = newResource;
		m_pool = &pool;

		if (oldPool)
			oldPool->RetireResource(oldResIndex);

		return ResultCode::kOK;
	}

	template<class T>
	void UniqueResourceRef<T>::Release(IResourcePool<T> *&outPool, T &outResource, size_t &outIndex)
	{
		outPool = m_pool;
		outResource = std::move(m_resource);
		outIndex = m_resIndex;

		m_pool = nullptr;
	}

	template<class T>
	void UniqueResourceRef<T>::Reset()
	{
		if (m_pool)
		{
			m_pool->RetireResource(m_resIndex);
			m_pool = nullptr;
		}
	}

	template<class T>
	bool UniqueResourceRef<T>::IsValid() const
	{
		return m_pool != nullptr;
	}

	template<class T>
	const T &UniqueResourceRef<T>::GetResource() const
	{
		return m_resource;
	}

	template<class T>
	IResourcePool<T> *UniqueResourceRef<T>::GetPool() const
	{
		return m_pool;
	}
} } } // rkit::render::vulkan
