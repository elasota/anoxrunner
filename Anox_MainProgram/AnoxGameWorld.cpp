#include "AnoxGameWorld.h"

#include "AnoxSpawnDefsResource.h"
#include "AnoxWorldObjectFactory.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Coroutine.h"
#include "rkit/Core/MemoryStream.h"

#include "anox/Data/EntityDef.h"

#include "anox/AnoxModule.h"
#include "anox/AnoxUtilitiesDriver.h"

namespace anox::game
{
	class WorldImpl : public rkit::OpaqueImplementation<World>
	{
	public:
		rkit::ResultCoroutine SpawnObjects(rkit::ICoroThread &thread, const AnoxSpawnDefsResourceBase *spawnDefsResource);
	};

	rkit::ResultCoroutine WorldImpl::SpawnObjects(rkit::ICoroThread &thread, const AnoxSpawnDefsResourceBase *spawnDefsResource)
	{
		anox::IUtilitiesDriver *anoxUtils = static_cast<anox::IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, u8"Utilities"));

		rkit::ReadOnlyMemoryStream dataStream(spawnDefsResource->GetDataBuffer());
		const data::EntityDefsSchema &schema = anoxUtils->GetEntityDefs();

		const WorldObjectSpawnParams spawnParams =
		{
			dataStream,
			spawnDefsResource->GetChunks()
		};

		for (const AnoxSpawnDefsResourceBase::SpawnDef &spawnDef : spawnDefsResource->GetSpawnDefs())
		{
			const uint32_t eclassIndex = spawnDef.m_eclass->m_entityClassIndex;
			if (eclassIndex >= schema.m_numClassDefs)
				CORO_THROW(rkit::ResultCode::kDataError);

			rkit::RCPtr<WorldObject> obj;
			CORO_CHECK(co_await WorldObjectFactory::SpawnWorldObject(thread, obj, spawnParams, *spawnDef.m_eclass, spawnDef.m_data));
		}

		int n = 0;

		CORO_RETURN_OK;
	}

	World::World()
	{
	}

	rkit::ResultCoroutine World::SpawnObjects(rkit::ICoroThread &thread, const AnoxSpawnDefsResourceBase *spawnDefsResource)
	{
		return Impl().SpawnObjects(thread, spawnDefsResource);
	}

	rkit::Result World::Create(rkit::UniquePtr<World> &outWorld)
	{
		return rkit::New<World>(outWorld);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::WorldImpl)
