#pragma once

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
}

namespace anox::game
{
	class WorldObject;
	class WorldImpl;
	struct WorldObjectSpawnParams;

	class WorldObjectContainer
	{
	public:
		WorldObjectContainer();

		friend class WorldObject;

		template<class T>
		static rkit::RCPtr<T> Wrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<T> &&obj);

	private:
		static void StaticStrongDelete(void *self) noexcept;
		static void StaticFinalDelete(void *self) noexcept;

		static rkit::RCPtr<WorldObject> InternalWrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<WorldObject> &&obj);

		rkit::WeakRefTracker m_weakRefTracker;

		rkit::UniquePtr<WorldObject> m_object;
		rkit::UniquePtr<WorldObjectContainer> m_self;
	};

	class WorldObject : public DynamicObject
	{
		ANOX_RTTI_CLASS(WorldObject, DynamicObject)

	public:
		friend class WorldImpl;

		rkit::RCPtr<WorldObject> GetRefCounted();
		rkit::RCPtr<const WorldObject> GetRefCounted() const;

	private:
		void SetContainer(WorldObjectContainer *container);

		WorldObjectContainer *m_container = nullptr;
		WorldObject *m_prevObject = nullptr;
		rkit::RCPtr<WorldObject> m_nextObject;
	};
}


namespace anox::game
{
	template<class T>
	rkit::RCPtr<T> WorldObjectContainer::Wrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<T> &&obj)
	{
		return InternalWrap(std::move(self), rkit::UniquePtr<WorldObject>(std::move(obj))).StaticCastMove<T>();
	}
}
