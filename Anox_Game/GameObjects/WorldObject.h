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

	class WorldObjectContainer
	{
	public:
		WorldObjectContainer();

		friend class WorldObject;

		template<class T>
		static rkit::RCPtr<T> Wrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<T> &&obj);

		template<class T>
		static rkit::WeakPtr<T> Weaken(const rkit::RCPtr<T> &ptr);

	private:
		static void StaticStrongDelete(void *self) noexcept;
		static void StaticFinalDelete(void *self) noexcept;

		static rkit::RCPtr<WorldObject> InternalWrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<WorldObject> &&obj);
		static rkit::WeakPtr<WorldObject> InternalWeaken(WorldObject *obj, rkit::RefCountedTracker *strongTracker);
		static rkit::WeakPtr<const WorldObject> InternalWeaken(const WorldObject *obj, rkit::RefCountedTracker *strongTracker);

		rkit::WeakRefTracker m_weakRefTracker;

		rkit::UniquePtr<WorldObject> m_object;
		rkit::UniquePtr<WorldObjectContainer> m_self;
	};

	class WorldObject : public DynamicObject
	{
		ANOX_RTTI_CLASS(WorldObject, DynamicObject)

	public:
		WorldObject();
		~WorldObject();

		friend class WorldImpl;
		friend class WorldObjectContainer;

		rkit::WeakPtr<WorldObject> GetNext() const;
		WorldObject* GetNextUnsafe() const;

		rkit::RCPtr<WorldObject> GetStrongRef();
		rkit::RCPtr<const WorldObject> GetStrongRef() const;

		rkit::WeakPtr<WorldObject> GetWeakRef();
		rkit::WeakPtr<const WorldObject> GetWeakRef() const;

		rkit::Result Initialize(World &world);
		virtual rkit::ResultCoroutine OnSpawnedFromLevel(rkit::ICoroThread &thread);

	protected:
		World &GetWorld() const;
		ScriptContext &GetScriptContext();

	private:
		World *m_world = nullptr;
		WorldObjectContainer *m_container = nullptr;

		WorldObject *m_prevObject = nullptr;
		rkit::RCPtr<WorldObject> m_nextObject;

		rkit::UniquePtr<ScriptContext> m_scriptContext;
	};
}

namespace anox::game
{
	template<class T>
	rkit::RCPtr<T> WorldObjectContainer::Wrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<T> &&obj)
	{
		return InternalWrap(std::move(self), rkit::UniquePtr<WorldObject>(std::move(obj))).StaticCastMove<T>();
	}

	template<class T>
	rkit::WeakPtr<T> WorldObjectContainer::Weaken(const rkit::RCPtr<T> &ptr)
	{
		return InternalWeaken(ptr.Get(), ptr.GetTracker()).template StaticCastMove<T>();
	}

	inline rkit::RCPtr<const WorldObject> WorldObject::GetStrongRef() const
	{
		return const_cast<WorldObject *>(this)->GetStrongRef();
	}

	inline rkit::WeakPtr<const WorldObject> WorldObject::GetWeakRef() const
	{
		return const_cast<WorldObject *>(this)->GetWeakRef();
	}

	inline World &WorldObject::GetWorld() const
	{
		return *m_world;
	}

	inline ScriptContext &WorldObject::GetScriptContext()
	{
		return *m_scriptContext;
	}
}
