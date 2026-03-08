#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	class AnoxSpawnDefsResourceBase;
}

namespace anox::game
{
	class WorldImpl;

	class World final : public rkit::Opaque<WorldImpl>
	{
	public:
		World();

		rkit::ResultCoroutine SpawnObjects(rkit::ICoroThread &thread, const AnoxSpawnDefsResourceBase *spawnDefs);

		static rkit::Result Create(rkit::UniquePtr<World> &outWorld);
	};
}
