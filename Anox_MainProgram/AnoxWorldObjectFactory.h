#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"

namespace rkit
{
	class ReadOnlyMemoryStream;

	template<class T>
	class RCPtr;
}

namespace anox::data
{
	struct EntityClassDef;
	struct EntitySpawnDataChunks;
}

namespace anox::game
{
	class WorldObject;
	class World;

	struct WorldObjectSpawnParams
	{
		const rkit::ReadOnlyMemoryStream &m_dataStream;
		const data::EntitySpawnDataChunks &m_chunks;
		World *m_world = nullptr;
	};

	template<class TEntityDefType>
	class WorldObjectInstantiator
	{
	public:
		static rkit::ResultCoroutine SpawnObject(rkit::ICoroThread &thread, rkit::RCPtr<WorldObject> &outObject, const WorldObjectSpawnParams &spawnParams, const TEntityDefType &spawnDef);
	};

	class WorldObjectFactory
	{
	public:
		static rkit::ResultCoroutine SpawnWorldObject(rkit::ICoroThread &thread, rkit::RCPtr<WorldObject> &outObject, const WorldObjectSpawnParams &spawnParams,
			const data::EntityClassDef &eclass, const void *data);
	};
}

