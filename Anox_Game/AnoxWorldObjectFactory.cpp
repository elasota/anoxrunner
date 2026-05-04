#include "AnoxWorldObjectFactory.h"

#include "rkit/Core/Coroutine.h"

#include "World.h"

#include "WorldObjectSpawnDispatcher.generated.inl"

namespace anox::game
{
	rkit::Result WorldObjectFactory::AddObjectToWorld(World &world, rkit::RCPtr<WorldObject> &&object)
	{
		return world.AddObject(std::move(object));
	}
}
