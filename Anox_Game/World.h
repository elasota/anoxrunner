#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class RCPtr;
}

namespace anox
{
	class AnoxSpawnDefsResourceBase;
}

namespace anox::data
{
	struct EClass_worldspawn;
}

namespace anox::game
{
	class WorldImpl;
	struct WorldObjectSpawnParams;
	class WorldObject;

	class World final : public rkit::Opaque<WorldImpl>
	{
	public:
		World();

		rkit::Result ApplyParams(const WorldObjectSpawnParams &spawnParams, const data::EClass_worldspawn &spawnDef);
		rkit::Result AddObject(rkit::RCPtr<WorldObject> &&obj);

		static rkit::Result Create(rkit::UniquePtr<World> &outWorld);
	};
}
