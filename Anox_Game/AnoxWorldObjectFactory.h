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

	template<class T>
	class UniquePtr;
}

namespace anox::data
{
	struct EntityClassDef2;
	struct EntitySpawnDataChunks;
}

namespace anox::game
{
	class WorldObject;
	class World;
	struct UserEntityDefValues;
	struct WorldObjectProxy;

	template<class T>
	struct ObjectFieldsBase;

	struct WorldObjectSpawnParams
	{
		rkit::Span<const rkit::ByteStringView> m_spawnDefStrings;
		rkit::Span<const UserEntityDefValues> m_udefs;
		rkit::Span<const rkit::ByteString> m_udefDescriptions;
	};

	typedef rkit::Result (*SerializeFromLevelFunction_t)(void *fieldsRef, const WorldObjectSpawnParams &spawnParams, const uint8_t *bytes);

	template<class TObjectType>
	class WorldObjectInstantiator
	{
	public:
		static rkit::Result CreateObject(rkit::UniquePtr<WorldObject> &outObject, ObjectFieldsBase<TObjectType> *&outFieldsRef);
		static rkit::Result LoadObjectFromLevel(ObjectFieldsBase<TObjectType> &object, const WorldObjectSpawnParams &spawnParams, const uint8_t *bytes);
	};

	class WorldObjectFactory
	{
	public:
		static rkit::Result CreateLevelObject(uint32_t levelObjectID, size_t &outSize, rkit::RCPtr<WorldObjectProxy> &outObject, void *&outFieldsRef, SerializeFromLevelFunction_t &outDeserializeFunction);

		template<class TObjClass>
		static rkit::Result CreateDynamic(World &world, TObjClass*& outObject);

		template<class TObjClass>
		static rkit::Result CreateLevelObjectTemplate(rkit::RCPtr<WorldObjectProxy> &outProxy, void *&outFieldsRef, SerializeFromLevelFunction_t &outDeserializeFunction);

	private:
		template<class TObjClass>
		static rkit::Result SerializeFromLevelCB(void *fieldsRef, const WorldObjectSpawnParams &spawnParams, const uint8_t *bytes);

		static rkit::Result AddObjectToWorld(World &world, rkit::RCPtr<WorldObjectProxy>&& object);
	};
}

#include "GameObjects/WorldObject.h"
#include "rkit/Core/Coroutine.h"

namespace anox::game
{
	template<class TObjClass>
	static rkit::Result WorldObjectFactory::CreateDynamic(World &world, TObjClass*& outObject)
	{
		rkit::RCPtr<WorldObjectProxy> proxy;
		RKIT_CHECK(rkit::New<WorldObjectProxy>(proxy));

		ObjectFieldsBase<TObjClass> *fieldsRef = nullptr;

		RKIT_CHECK(WorldObjectInstantiator<TObjClass>::CreateObject(proxy->m_object, fieldsRef));

		TObjClass *objPtr = static_cast<TObjClass *>(proxy->m_object.Get());

		RKIT_CHECK(AddObjectToWorld(world, std::move(proxy)));

		WorldObject *wo = objPtr;
		RKIT_CHECK(wo->Initialize(world));

		outObject = objPtr;
		RKIT_RETURN_OK;
	}

	template<class TObjClass>
	rkit::Result WorldObjectFactory::CreateLevelObjectTemplate(rkit::RCPtr<WorldObjectProxy> &outObjectProxy, void *&outFieldsRef, SerializeFromLevelFunction_t &outDeserializeFunction)
	{
		rkit::RCPtr<WorldObjectProxy> proxy;
		RKIT_CHECK(rkit::New<WorldObjectProxy>(proxy));

		ObjectFieldsBase<TObjClass> *fieldsRef = nullptr;
		RKIT_CHECK(WorldObjectInstantiator<TObjClass>::CreateObject(proxy->m_object, fieldsRef));

		outObjectProxy = std::move(proxy);
		outDeserializeFunction = SerializeFromLevelCB<TObjClass>;
		outFieldsRef = static_cast<void *>(fieldsRef);
		RKIT_RETURN_OK;
	}

	template<class TObjClass>
	rkit::Result WorldObjectFactory::SerializeFromLevelCB(void *fieldsRef, const WorldObjectSpawnParams &spawnParams, const uint8_t *bytes)
	{
		return WorldObjectInstantiator<TObjClass>::LoadObjectFromLevel(*static_cast<ObjectFieldsBase<TObjClass> *>(fieldsRef), spawnParams, bytes);
	}
};


#define ANOX_IMPLEMENT_WORLD_OBJECT_INSTANTIATOR(objClass) \
template<>\
rkit::Result anox::game::WorldObjectInstantiator<objClass>::CreateObject(rkit::UniquePtr<WorldObject> &outObject, ObjectFieldsBase<objClass> *&outFieldsRef)\
{\
	rkit::UniquePtr<objClass> obj;\
	RKIT_CHECK(rkit::New<objClass>(obj));\
	outFieldsRef = ::anox::game::priv::PrivateAccessor::ImplicitCast<ObjectFieldsBase<objClass>>(obj.Get());\
	outObject = std::move(obj);\
	RKIT_RETURN_OK;\
}
