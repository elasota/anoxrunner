#include "WorldObject.h"

#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	WorldObjectContainer::WorldObjectContainer()
		: m_weakRefTracker(StaticStrongDelete, StaticFinalDelete, this)
	{
	}

	void WorldObjectContainer::StaticStrongDelete(void *self) noexcept
	{
		WorldObjectContainer *container = static_cast<WorldObjectContainer *>(self);
		container->m_object.Reset();
	}

	void WorldObjectContainer::StaticFinalDelete(void *self) noexcept
	{
		WorldObjectContainer *container = static_cast<WorldObjectContainer *>(self);
		rkit::UniquePtr<WorldObjectContainer> selfRef = std::move(container->m_self);
		selfRef.Reset();
	}

	rkit::RCPtr<WorldObject> WorldObjectContainer::InternalWrap(rkit::UniquePtr<WorldObjectContainer> &&self, rkit::UniquePtr<WorldObject> &&obj)
	{
		RKIT_ASSERT(self.IsValid());
		RKIT_ASSERT(obj.IsValid());

		WorldObjectContainer &container = *self;
		WorldObject &wo = *obj;

		container.m_self = std::move(self);
		container.m_object = std::move(obj);

		wo.m_container = &container;

		return rkit::RCPtr<WorldObject>(&wo, &container.m_weakRefTracker.GetStrongTracker());
	}

	rkit::WeakPtr<WorldObject> WorldObjectContainer::InternalWeaken(WorldObject *obj, rkit::RefCountedTracker *strongTracker)
	{
		return rkit::WeakPtr<WorldObject>(obj, rkit::WeakRefTracker::FromStrongTracker(strongTracker));
	}

	rkit::WeakPtr<const WorldObject> WorldObjectContainer::InternalWeaken(const WorldObject *obj, rkit::RefCountedTracker *strongTracker)
	{
		return InternalWeaken(const_cast<WorldObject *>(obj), strongTracker);
	}

	rkit::WeakPtr<WorldObject> WorldObject::GetNext() const
	{
		return WorldObjectContainer::Weaken(m_nextObject);
	}

	WorldObject *WorldObject::GetNextUnsafe() const
	{
		return m_nextObject.Get();
	}

	rkit::RCPtr<WorldObject> WorldObject::GetStrongRef()
	{
		return rkit::RCPtr<WorldObject>(this, &m_container->m_weakRefTracker.GetStrongTracker());
	}

	rkit::WeakPtr<WorldObject> WorldObject::GetWeakRef()
	{
		return rkit::WeakPtr<WorldObject>(this, &m_container->m_weakRefTracker);
	}
}
