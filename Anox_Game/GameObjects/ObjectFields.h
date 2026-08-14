#pragma once

#include "rkit/Data/ContentID.h"
#include "rkit/Core/RefCounted.h"


namespace anox::game::priv
{
	template<class T>
	struct ObjectRTTIResolver
	{
	};

	// This should only ever be used by codegen files!
	template<class T>
	struct ObjectFieldsImpl
	{
	};

	// This should only ever be used by codegen files!
	template<class T>
	struct ObjectRTTIImpl
	{
	};
}

namespace anox::game
{
	class World;
	class ScenePackage;

	template<class TResource>
	class GameResourceHandle
	{
	public:
		using Resource_t = TResource;

		GameResourceHandle() = default;
		explicit GameResourceHandle(rkit::RCPtr<TResource> resource);

		TResource *Get() const;
		TResource *operator->() const;

	private:
		rkit::RCPtr<TResource> m_res;
	};

	using SceneHandle = GameResourceHandle<ScenePackage>;

	template<class T>
	struct ObjectFieldsBase
	{
	};

	template<class T>
	using ObjectFields = typename priv::ObjectRTTIResolver<T>::FieldType_t;

	template<class T>
	using ObjectRTTI = typename priv::ObjectRTTIResolver<T>::RTTIType_t;

	template<class THandleType>
	class ResourceRef
	{
	public:
		using Resource_t = typename THandleType::Resource_t;

		ResourceRef() = default;
		ResourceRef(const ResourceRef &other) = default;
		ResourceRef(ResourceRef &&other) noexcept = default;
		explicit ResourceRef(THandleType handle);

		ResourceRef &operator=(const ResourceRef &other) = default;
		ResourceRef &operator=(ResourceRef &&other) noexcept = default;

		void ReleaseResourceRef(World &world);
		const THandleType &GetHandle() const;

		Resource_t *Get() const;
		Resource_t *operator->() const;

	private:
		THandleType m_handle;
	};
}

#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	template<class TResource>
	GameResourceHandle<TResource>::GameResourceHandle(rkit::RCPtr<TResource> resource)
		: m_res(std::move(resource))
	{
	}

	template<class TResource>
	TResource *GameResourceHandle<TResource>::Get() const
	{
		return m_res.Get();
	}

	template<class TResource>
	TResource *GameResourceHandle<TResource>::operator->() const
	{
		RKIT_ASSERT(m_res.Get() != nullptr);
		return m_res.Get();
	}

	template<class THandleType>
	ResourceRef<THandleType>::ResourceRef(THandleType handle)
		: m_handle(std::move(handle))
	{
	}

	template<class THandleType>
	const THandleType &ResourceRef<THandleType>::GetHandle() const
	{
		return m_handle;
	}

	template<class THandleType>
	ResourceRef<THandleType>::Resource_t *ResourceRef<THandleType>::Get() const
	{
		return m_handle.Get();
	}

	template<class THandleType>
	ResourceRef<THandleType>::Resource_t *ResourceRef<THandleType>::operator->() const
	{
		ResourceRef<THandleType>::Resource_t *res = m_handle.Get();
		RKIT_ASSERT(res != nullptr);
		return res;
	}
}
