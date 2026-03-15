#include "AnoxGameWorld.h"

#include "AnoxSpawnDefsResource.h"
#include "AnoxWorldObjectFactory.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Coroutine.h"
#include "rkit/Core/MemoryStream.h"

#include "anox/CoreUtils/CoreUtils.h"
#include "anox/Data/EntityDef.h"

#include "anox/AnoxModule.h"
#include "anox/AnoxUtilitiesDriver.h"

namespace anox::game
{
	class WorldImpl : public rkit::OpaqueImplementation<World>
	{
	public:
	};

	World::World()
	{
	}

	rkit::Result World::Create(rkit::UniquePtr<World> &outWorld)
	{
		return rkit::New<World>(outWorld);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::WorldImpl)
