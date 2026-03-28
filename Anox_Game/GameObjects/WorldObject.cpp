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

		return rkit::RCPtr<WorldObject>(&wo, &container.m_weakRefTracker.GetStrongTracker());
	}
}
