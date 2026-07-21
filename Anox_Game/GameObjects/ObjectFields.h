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
		GameResourceHandle() = default;

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
		ResourceRef() = default;
		ResourceRef(const ResourceRef &other) = default;
		ResourceRef(ResourceRef &&other) = default;

		void ReleaseResourceRef(World &world);

	private:
		THandleType m_handle;
	};
}
