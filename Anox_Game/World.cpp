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
#include "MusicManager.h"

namespace anox::game
{
	class WorldImpl : public rkit::OpaqueImplementation<World>
	{
	public:
		friend class World;

		explicit WorldImpl(ScriptManager &scriptManager);
		~WorldImpl();

		rkit::Result Initialize();

		void AddObject(rkit::RCPtr<WorldObjectProxy> &&obj);
		void RemoveObject(WorldObject *obj);

		AllWorldObjectsCollection GetAllObjects() const;

		rkit::ResultCoroutine OnWorldStarted(rkit::ICoroThread &thread);
		rkit::ResultCoroutine OnRunFrame(rkit::ICoroThread &thread);

	private:
		void CleanUpObject(WorldObjectProxy *obj);

		ObjRef<GlobalSingleton> m_globalSingleton;
		ScriptManager &m_scriptManager;
		rkit::UniquePtr<ScriptEnvironment> m_scriptEnvironment;
		rkit::UniquePtr<MusicManager> m_musicManager;

		rkit::RCPtr<WorldObjectProxy> m_firstObj;
		WorldObjectProxy* m_lastObject = nullptr;

		WorldObjectProxy *m_firstDead = nullptr;
	};

	WorldImpl::WorldImpl(ScriptManager &scriptManager)
		: m_scriptManager(scriptManager)
	{
	}

	WorldImpl::~WorldImpl()
	{
		while (m_firstObj.IsValid())
			m_firstObj = std::move(m_firstObj->m_next);
	}

	rkit::Result WorldImpl::Initialize()
	{
		RKIT_CHECK(m_scriptManager.CreateScriptEnvironment(m_scriptEnvironment));
		RKIT_CHECK(MusicManager::Create(m_musicManager));

		RKIT_RETURN_OK;
	}

	void WorldImpl::AddObject(rkit::RCPtr<WorldObjectProxy> &&objProxyRCPtr)
	{
		WorldObjectProxy *proxyPtr = objProxyRCPtr.Get();

		WorldObject *obj = proxyPtr->m_object.Get();

		RKIT_ASSERT(obj != nullptr);

		obj->m_proxy = proxyPtr;
		obj->m_world = &this->Base();

		if (m_lastObject)
			m_lastObject->m_next = std::move(objProxyRCPtr);
		else
			m_firstObj = std::move(objProxyRCPtr);

		m_lastObject = proxyPtr;
	}

	void WorldImpl::RemoveObject(WorldObject *obj)
	{
		WorldObjectProxy *proxy = obj->m_proxy;

		RKIT_ASSERT(proxy->m_object.IsValid());

		proxy->m_object.Reset();

		proxy->m_nextDead = m_firstDead;
		m_firstDead = proxy;
	}

	void WorldImpl::CleanUpObject(WorldObjectProxy *proxy)
	{
		WorldObjectProxy *nextProxyPtr = proxy->m_next.Get();
		WorldObjectProxy *prevProxy = proxy->m_prev;

		if (nextProxyPtr)
			nextProxyPtr->m_prev = prevProxy;
		else
			m_lastObject = prevProxy;

		// This will destroy the object!  Do not reference it after this line!
		if (prevProxy)
			prevProxy->m_next = std::move(proxy->m_next);
		else
			m_firstObj = std::move(proxy->m_next);
	}

	AllWorldObjectsCollection WorldImpl::GetAllObjects() const
	{
		WorldObjectProxy *firstValidProxy = m_firstObj.Get();

		while (firstValidProxy != nullptr && !firstValidProxy->m_object.IsValid())
			firstValidProxy = firstValidProxy->m_next.Get();

		return AllWorldObjectsCollection(firstValidProxy);
	}

	rkit::ResultCoroutine WorldImpl::OnWorldStarted(rkit::ICoroThread &thread)
	{
		for (WorldObject &obj : GetAllObjects())
		{
			if (GlobalSingleton *singleton = DynamicCast<GlobalSingleton>(&obj))
			{
				m_globalSingleton = singleton;
				break;
			}
		}

		for (WorldObject &obj : GetAllObjects())
		{
			CORO_CHECK(co_await obj.OnSpawnedFromLevel(thread));
		}

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine WorldImpl::OnRunFrame(rkit::ICoroThread &thread)
	{
		for (WorldObject &obj : GetAllObjects())
		{
			CORO_CHECK(co_await obj.OnFrame(thread));
		}

		CORO_CHECK(m_musicManager->OnFrame());

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

	rkit::Result World::AddObject(rkit::RCPtr<WorldObjectProxy> &&obj)
	{
		Impl().AddObject(std::move(obj));
		RKIT_RETURN_OK;
	}

	void World::RemoveObject(WorldObject *obj)
	{
		Impl().RemoveObject(obj);
	}

	AllWorldObjectsCollection World::GetAllObjects() const
	{
		return Impl().GetAllObjects();
	}

	rkit::ResultCoroutine World::OnWorldStarted(rkit::ICoroThread &thread)
	{
		return Impl().OnWorldStarted(thread);
	}

	rkit::ResultCoroutine World::OnRunFrame(rkit::ICoroThread &thread)
	{
		return Impl().OnRunFrame(thread);
	}

	ScriptManager &World::GetScriptManager() const
	{
		return Impl().m_scriptManager;
	}

	ScriptEnvironment &World::GetScriptEnvironment() const
	{
		return *Impl().m_scriptEnvironment;
	}

	MusicManager &World::GetMusicManager() const
	{
		return *Impl().m_musicManager;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::WorldImpl)
