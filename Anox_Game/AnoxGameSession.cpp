#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroThread.h"
#include "rkit/Core/RefCounted.h"

#include "anox/Data/EntityDef.h"
#include "anox/Game/SpawnDef.h"
#include "anox/CoreUtils/CoreUtils.h"

#include "AnoxGameSession.h"
#include "AnoxWorldObjectFactory.h"

#include "World.h"

namespace anox::game
{
	class World;

	class SessionImpl final : public rkit::OpaqueImplementation<Session>
	{
	public:
		friend class Session;

		SessionImpl();

		rkit::Result Initialize();

		rkit::ResultCoroutine SpawnInitialEntities(rkit::ICoroThread &thread, World &world, rkit::Span<const SpawnDef> spawnDefs,
			rkit::Span<const uint8_t> spawnData, rkit::Span<const rkit::ByteStringView> spawnDefStrings,
			rkit::Span<const game::UserEntityDefValues> udefs, rkit::Span<const rkit::ByteString> udefDescriptions);

		rkit::ResultCoroutine PostSpawnInitialEntities(rkit::ICoroThread &thread, World &world);
		rkit::ResultCoroutine EnterGameSession(rkit::ICoroThread &thread, World &world);

		rkit::ResultCoroutine RunFrame(rkit::ICoroThread &thread, World &world);

	private:
		rkit::UniquePtr<World> m_world;
		rkit::UniquePtr<rkit::ICoroThread> m_mainCoroThread;
	};
}

namespace anox::game
{
	SessionImpl::SessionImpl()
	{
	}

	rkit::Result SessionImpl::Initialize()
	{
		rkit::IMallocDriver *allocDriver = rkit::GetDrivers().m_mallocDriver;
		RKIT_CHECK(rkit::utils::CreateCoroThread(m_mainCoroThread, allocDriver, 1 * 1024 * 1024, rkit::GetDrivers().GetAssertDriver()));

		RKIT_CHECK(World::Create(m_world));

		RKIT_RETURN_OK;
	}

	rkit::ResultCoroutine SessionImpl::SpawnInitialEntities(rkit::ICoroThread &thread, World &world,
		rkit::Span<const SpawnDef> spawnDefs,
		rkit::Span<const uint8_t> spawnData, rkit::Span<const rkit::ByteStringView> spawnDefStrings,
		rkit::Span<const game::UserEntityDefValues> udefs, rkit::Span<const rkit::ByteString> udefDescriptions)
	{
		const data::EntityDefsSchema &schema = anox::utils::GetEntityDefs();

		for (const SpawnDef &spawnDef : spawnDefs)
		{
			const data::EntityClassDef *eclass = schema.m_classDefs[spawnDef.m_eclassIndex];
			const rkit::Span<const uint8_t> itemSpawnData = spawnData.SubSpan(spawnDef.m_dataOffset, eclass->m_structSize);

			WorldObjectSpawnParams spawnParams =
			{
				world,
				spawnDefStrings,
				udefs,
				udefDescriptions,
				*eclass,
				itemSpawnData
			};

			rkit::RCPtr<WorldObject> obj;
			CORO_CHECK(co_await WorldObjectFactory::SpawnWorldObject(thread, obj, spawnParams));

			if (obj.IsValid())
			{
				CORO_CHECK(world.AddObject(std::move(obj)));
			}
		}

		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine SessionImpl::PostSpawnInitialEntities(rkit::ICoroThread &thread, World &world)
	{
		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine SessionImpl::EnterGameSession(rkit::ICoroThread &thread, World &world)
	{
		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine SessionImpl::RunFrame(rkit::ICoroThread &thread, World &world)
	{
		CORO_RETURN_OK;
	}

	rkit::Result Session::AsyncSpawnInitialEntities(rkit::Span<const SpawnDef> spawnDefs, World &world,
		rkit::Span<const uint8_t> spawnData,
		rkit::Span<const rkit::ByteStringView> spawnDefStrings, rkit::Span<const game::UserEntityDefValues> udefs,
		rkit::Span<const rkit::ByteString> udefDescriptions)
	{
		rkit::ICoroThread &thread = *Impl().m_mainCoroThread;
		return thread.EnterFunction(Impl().SpawnInitialEntities(thread, world, spawnDefs, spawnData, spawnDefStrings, udefs, udefDescriptions));
	}

	rkit::Result Session::AsyncPostSpawnInitialEntities(World &world)
	{
		rkit::ICoroThread &thread = *Impl().m_mainCoroThread;
		return thread.EnterFunction(Impl().PostSpawnInitialEntities(thread, world));
	}

	rkit::Result Session::AsyncEnterGameSession(World &world)
	{
		rkit::ICoroThread &thread = *Impl().m_mainCoroThread;
		return thread.EnterFunction(Impl().EnterGameSession(thread, world));
	}

	rkit::Result Session::AsyncRunFrame(World &world)
	{
		rkit::ICoroThread &thread = *Impl().m_mainCoroThread;
		return thread.EnterFunction(Impl().RunFrame(thread, world));
	}

	World &Session::GetWorld() const
	{
		return *Impl().m_world;
	}

	rkit::Result Session::WaitForMainThreadCoro(bool &outIsFinished)
	{
		rkit::ICoroThread &mainCoroThread = *Impl().m_mainCoroThread;

		for (;;)
		{
			switch (mainCoroThread.GetState())
			{
			case rkit::CoroThreadState::kInactive:
				outIsFinished = true;
				RKIT_RETURN_OK;
			case rkit::CoroThreadState::kBlocked:
				outIsFinished = false;
				RKIT_RETURN_OK;
			case rkit::CoroThreadState::kSuspended:
				RKIT_CHECK(mainCoroThread.Resume());
				break;
			default:
				RKIT_THROW(rkit::ResultCode::kInternalError);
			}
		}
	}

	rkit::Result Session::Create(rkit::UniquePtr<Session> &outSession, rkit::IMallocDriver *alloc)
	{
		rkit::UniquePtr<Session> session;
		rkit::NewWithAlloc<Session>(session, alloc);

		RKIT_CHECK(session->Impl().Initialize());

		outSession = std::move(session);

		RKIT_RETURN_OK;
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::SessionImpl)
