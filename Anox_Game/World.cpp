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
#include "rkit/Core/Vector.h"

#include "GameObjects/Serializable.h"
#include "GameObjects/GlobalSingleton.h"

#include "AllWorldObjects.h"
#include "ScriptEnvironment.h"
#include "ScriptManager.h"

namespace anox::game
{
	class WorldImpl : public rkit::OpaqueImplementation<World>
	{
	public:
		friend class World;

		explicit WorldImpl(ScriptManager &scriptManager);
		~WorldImpl();

		rkit::Result Initialize();

		void AddObject(rkit::RCPtr<WorldObject> &&obj);
		void RemoveObject(WorldObject *obj);

		rkit::WeakPtr<WorldObject> GetFirstObject() const;
		WorldObject* GetFirstObjectUnsafe() const;

		AllWorldObjectsCollection GetAllObjects() const;

		rkit::ResultCoroutine OnWorldStarted(rkit::ICoroThread &thread);

	private:
		rkit::WeakPtr<GlobalSingleton> m_globalSingleton;
		ScriptManager &m_scriptManager;
		rkit::UniquePtr<ScriptEnvironment> m_scriptEnvironment;

		rkit::RCPtr<WorldObject> m_firstObj;
		WorldObject *m_lastObject = nullptr;
	};

	WorldImpl::WorldImpl(ScriptManager &scriptManager)
		: m_scriptManager(scriptManager)
	{
	}

	WorldImpl::~WorldImpl()
	{
		while (m_firstObj.IsValid())
			m_firstObj = std::move(m_firstObj->m_nextObject);
	}

	rkit::Result WorldImpl::Initialize()
	{
		RKIT_CHECK(m_scriptManager.CreateScriptEnvironment(m_scriptEnvironment));

		RKIT_RETURN_OK;
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

	rkit::WeakPtr<WorldObject> WorldImpl::GetFirstObject() const
	{
		return WorldObjectContainer::Weaken(m_firstObj);
	}

	WorldObject *WorldImpl::GetFirstObjectUnsafe() const
	{
		return m_firstObj.Get();
	}

	AllWorldObjectsCollection WorldImpl::GetAllObjects() const
	{
		return AllWorldObjectsCollection(GetFirstObjectUnsafe());
	}

	rkit::ResultCoroutine WorldImpl::OnWorldStarted(rkit::ICoroThread &thread)
	{
		{
			rkit::WeakPtr<GlobalSingleton> globalSingleton;

			for (WorldObject *obj : GetAllObjects())
			{
				if (GlobalSingleton *singleton = DynamicCast<GlobalSingleton>(obj))
				{
					globalSingleton = singleton->GetWeakRef().StaticCastMove<GlobalSingleton>();
					break;
				}
			}

			m_globalSingleton = std::move(globalSingleton);
		}

		rkit::Vector<rkit::WeakPtr<WorldObject>> objs;
		for (WorldObject *obj : GetAllObjects())
		{
			CORO_CHECK(objs.Append(obj->GetWeakRef()));
		}

		for (const rkit::WeakPtr<WorldObject> &objWeak : objs)
		{
			rkit::RCPtr<WorldObject> lockedObj = objWeak.Lock();

			if (lockedObj.IsValid())
			{
				CORO_CHECK(co_await lockedObj->OnSpawnedFromLevel(thread));
			}
		}

		CORO_RETURN_OK;
	}

	// ------------------------------------------------------------------
	// Public API
	World::World(ScriptManager &scriptManager)
		: Opaque<WorldImpl>(scriptManager)
	{
	}

	rkit::Result World::Create(rkit::UniquePtr<World> &outWorld, ScriptManager &scriptManager)
	{
		rkit::UniquePtr<World> world;
		RKIT_CHECK(rkit::New<World>(world, scriptManager));
		RKIT_CHECK(world->Impl().Initialize());

		outWorld = std::move(world);

		RKIT_RETURN_OK;
	}

	rkit::Result World::AddObject(rkit::RCPtr<WorldObject> &&obj)
	{
		Impl().AddObject(std::move(obj));
		RKIT_RETURN_OK;
	}

	rkit::WeakPtr<WorldObject> World::GetFirstObject() const
	{
		return Impl().GetFirstObject();
	}

	WorldObject *World::GetFirstObjectUnsafe() const
	{
		return Impl().GetFirstObjectUnsafe();
	}

	AllWorldObjectsCollection World::GetAllObjects() const
	{
		return Impl().GetAllObjects();
	}

	rkit::ResultCoroutine World::OnWorldStarted(rkit::ICoroThread &thread)
	{
		return Impl().OnWorldStarted(thread);
	}

	ScriptManager &World::GetScriptManager() const
	{
		return Impl().m_scriptManager;
	}

	ScriptEnvironment &World::GetScriptEnvironment() const
	{
		return *Impl().m_scriptEnvironment;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::WorldImpl)
