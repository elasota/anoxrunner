#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/WeakRef.h"

#include "DynamicObject.h"

#define ANOX_OBJECT_RTTI_BASE(type)

namespace rkit
{
	class WeakRefTracker;

	template<class T>
	class RCPtr;

	template<class T>
	class WeakPtr;
}

namespace anox::game
{
	class ScriptContext;
	class WorldObject;
	class World;
	class WorldImpl;
	struct WorldObjectSpawnParams;
	struct WorldObjectProxy;

	template<class T>
	class ObjRef final
	{
	public:
		template<class TOther>
		friend class ObjRef;

		ObjRef() = default;

		ObjRef(T *ptr);

		template<class TOther>
		ObjRef(const ObjRef<TOther> &otherRef);

		template<class TOther>
		ObjRef(ObjRef<TOther> &&otherRef);

		ObjRef(const ObjRef<T> &) = default;
		ObjRef(ObjRef<T> &&) = default;

		ObjRef &operator=(const ObjRef &) = default;
		ObjRef &operator=(ObjRef &&) = default;

		T *GetObject() const;

		template<class TOther>
		ObjRef<TOther> DynamicCast() const;

		template<class TOther>
		ObjRef<TOther> DynamicCastMove();

		template<class TOther>
		ObjRef<TOther> StaticCast() const;

		template<class TOther>
		ObjRef<TOther> StaticCastMove();

	private:
		explicit ObjRef(const rkit::RCPtr<WorldObjectProxy> &proxy);
		explicit ObjRef(rkit::RCPtr<WorldObjectProxy> &&proxy);

		rkit::RCPtr<WorldObjectProxy> m_proxy;
	};

	struct WorldObjectProxy final : public rkit::RefCounted
	{
		WorldObjectProxy *m_prev = nullptr;
		rkit::RCPtr<WorldObjectProxy> m_next;
		rkit::UniquePtr<WorldObject> m_object;

		WorldObjectProxy *m_nextDead = nullptr;
	};

	class WorldObject : public DynamicObject
	{
		ANOX_RTTI_CLASS(WorldObject, DynamicObject)

	public:
		WorldObject();
		~WorldObject();

		friend class WorldImpl;

		virtual rkit::Result Initialize(World &world);
		virtual rkit::ResultCoroutine OnSpawnedFromLevel(rkit::ICoroThread &thread);
		virtual rkit::ResultCoroutine OnFrame(rkit::ICoroThread &thread);

		WorldObjectProxy &GetProxy() const;

	protected:
		World &GetWorld() const;
		ScriptContext &GetScriptContext();

	private:
		World *m_world = nullptr;
		WorldObjectProxy *m_proxy = nullptr;

		rkit::UniquePtr<ScriptContext> m_scriptContext;
	};
}

#include <type_traits>

namespace anox::game
{
	template<class T>
	template<class TOther>
	inline ObjRef<T>::ObjRef(const ObjRef<TOther> &otherRef)
		: m_proxy(otherRef.m_proxy)
	{
		static_assert(std::is_convertible<T *, TOther *>::value, "Reference is not implicitly convertible");
	}


	template<class T>
	ObjRef<T>::ObjRef(T *ptr)
		: m_proxy(ptr == nullptr ? nullptr : &static_cast<const WorldObject *>(ptr)->GetProxy())
	{
	}

	template<class T>
	template<class TOther>
	inline ObjRef<T>::ObjRef(ObjRef<TOther> &&otherRef)
		: m_proxy(std::move(otherRef.m_proxy))
	{
		static_assert(std::is_convertible<T *, TOther *>::value, "Reference is not implicitly convertible");
	}

	template<class T>
	T *ObjRef<T>::GetObject() const
	{
		return m_proxy.IsValid() ? static_cast<T *>(this->m_proxy.Get()->m_object.Get()) : nullptr;
	}

	template<class T>
	template<class TOther>
	ObjRef<TOther> ObjRef<T>::DynamicCast() const
	{
		TOther *otherRef = game::DynamicCast<TOther>(this->GetObject());
		if (otherRef)
			return ObjRef<TOther>(m_proxy);

		return ObjRef<TOther>();
	}

	template<class T>
	template<class TOther>
	ObjRef<TOther> ObjRef<T>::DynamicCastMove()
	{
		TOther *otherRef = game::DynamicCast<TOther>(this->GetObject());
		if (otherRef)
			return ObjRef<TOther>(std::move(m_proxy));

		return ObjRef<TOther>();
	}

	template<class T>
	template<class TOther>
	ObjRef<TOther> ObjRef<T>::StaticCast() const
	{
		return ObjRef<TOther>(m_proxy);
	}

	template<class T>
	template<class TOther>
	ObjRef<TOther> ObjRef<T>::StaticCastMove()
	{
		return ObjRef<TOther>(std::move(m_proxy));
	}

	inline World &WorldObject::GetWorld() const
	{
		return *m_world;
	}

	inline ScriptContext &WorldObject::GetScriptContext()
	{
		return *m_scriptContext;
	}

	inline WorldObjectProxy &WorldObject::GetProxy() const
	{
		return *m_proxy;
	}
}
