#pragma once

#include "rkit/Core/CoroutineProtos.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/Span.h"
#include "rkit/Core/StringProto.h"

namespace rkit
{
	class ReadOnlyMemoryStream;

	template<class T>
	class RCPtr;

	template<class T>
	class Span;
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
	struct UserEntityDefValues;

	struct WorldObjectSpawnParams
	{
		World &m_world;
		rkit::Span<const rkit::ByteStringView> m_spawnDefStrings;
		rkit::Span<const UserEntityDefValues> m_udefs;
		rkit::Span<const rkit::ByteString> m_udefDescriptions;
		const data::EntityClassDef &m_eclass;
		rkit::Span<const uint8_t> m_data;
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
		static rkit::ResultCoroutine SpawnWorldObject(rkit::ICoroThread &thread, rkit::RCPtr<WorldObject> &outObject, const WorldObjectSpawnParams &spawnParams);

		template<class TObjClass, class TEClass>
		static rkit::ResultCoroutine SpawnObjectTemplate(rkit::ICoroThread &thread, rkit::RCPtr<anox::game::WorldObject> &outObject, const anox::game::WorldObjectSpawnParams &spawnParams, const TEClass &spawnDef);
	};
}

#include "GameObjects/AnoxWorldObject.h"
#include "rkit/Core/Coroutine.h"

namespace anox::game
{
	template<class TObjClass, class TEClass>
	rkit::ResultCoroutine WorldObjectFactory::SpawnObjectTemplate(rkit::ICoroThread &thread, rkit::RCPtr<anox::game::WorldObject> &outObject, const anox::game::WorldObjectSpawnParams &spawnParams, const TEClass &spawnDef)
	{
		rkit::UniquePtr<WorldObjectContainer> container;
		CORO_CHECK(rkit::New<WorldObjectContainer>(container));
		rkit::UniquePtr<TObjClass> obj;
		CORO_CHECK(rkit::New<TObjClass>(obj));
		CORO_CHECK(co_await obj->Initialize(thread, spawnParams, spawnDef));
		outObject = WorldObjectContainer::Wrap(std::move(container), std::move(obj));
		CORO_RETURN_OK;
	}
};

#define ANOX_IMPLEMENT_WORLD_OBJECT_INSTANTIATOR(objClass, eclass) \
template<>\
rkit::ResultCoroutine anox::game::WorldObjectInstantiator<::anox::data::EClass_ ## eclass>::SpawnObject(rkit::ICoroThread &thread, rkit::RCPtr<anox::game::WorldObject> &outObject, const anox::game::WorldObjectSpawnParams &spawnParams, const anox::data::EClass_ ## eclass &spawnDef)\
{\
	return ::anox::game::WorldObjectFactory::SpawnObjectTemplate<objClass, ::anox::data::EClass_ ## eclass>(thread, outObject, spawnParams, spawnDef);\
}
