#include "World.h"
#include "AnoxWorldObjectFactory.h"

#include "anox/CoreUtils/CoreUtils.h"

#include "anox/Data/EntityDef.h"
#include "anox/Data/EntityStructs.h"

#include "anox/AnoxModule.h"
#include "anox/AnoxUtilitiesDriver.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Coroutine.h"
#include "rkit/Core/MemoryStream.h"

#include "GameObjects/Serializable.h"

namespace anox::game
{
	class WorldImpl : public rkit::OpaqueImplementation<World>
	{
	public:
		~WorldImpl();

		rkit::Result ApplyParams(const WorldObjectSpawnParams &spawnParams, const data::EClass_worldspawn &spawnDef);
		void AddObject(rkit::RCPtr<WorldObject> &&obj);
		void RemoveObject(WorldObject *obj);

	private:
		Label m_sequence;

		rkit::RCPtr<WorldObject> m_firstObj;
		WorldObject *m_lastObject;
	};

	rkit::Result WorldImpl::ApplyParams(const WorldObjectSpawnParams &spawnParams, const data::EClass_worldspawn &spawnDef)
	{
		m_sequence = spawnDef.m_sequence;
		RKIT_RETURN_OK;
	}

	WorldImpl::~WorldImpl()
	{
		while (m_firstObj.IsValid())
			m_firstObj = std::move(m_firstObj->m_nextObject);
	}

	void WorldImpl::AddObject(rkit::RCPtr<WorldObject> &&obj)
	{
		WorldObject *objPtr = obj.Get();
		if (m_lastObject)
			m_lastObject->m_nextObject = std::move(obj);
		else
			m_firstObj = std::move(obj);

		m_lastObject = objPtr;
	}

	void WorldImpl::RemoveObject(WorldObject *obj)
	{
		WorldObject *nextObjPtr = obj->m_nextObject.Get();
		WorldObject *prevObj = obj->m_prevObject;

		if (nextObjPtr)
			nextObjPtr->m_prevObject = prevObj;
		else
			m_lastObject = prevObj;

		// This will destroy the object!  Do not reference it after this line!
		if (prevObj)
			prevObj->m_nextObject = std::move(obj->m_nextObject);
		else
			m_firstObj = std::move(obj->m_nextObject);
	}

	World::World()
	{
	}

	rkit::Result World::Create(rkit::UniquePtr<World> &outWorld)
	{
		return rkit::New<World>(outWorld);
	}

	rkit::Result World::ApplyParams(const WorldObjectSpawnParams &spawnParams, const data::EClass_worldspawn &spawnDef)
	{
		return Impl().ApplyParams(spawnParams, spawnDef);
	}

	rkit::Result World::AddObject(rkit::RCPtr<WorldObject> &&obj)
	{
		return Impl().AddObject(std::move(obj));
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::WorldImpl)
