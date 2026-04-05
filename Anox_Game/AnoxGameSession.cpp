#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Coroutine.h"
#include "rkit/Core/CoroThread.h"
#include "rkit/Core/RefCounted.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "anox/Data/EntityDef.h"
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

		rkit::ResultCoroutine SpawnInitialEntities(rkit::ICoroThread &thread, World &world, rkit::Span<const rkit::endian::LittleUInt32_t> spawnDefs,
			rkit::Span<const uint8_t> spawnData, rkit::Vector<rkit::ByteStringView> spawnDefStrings,
			rkit::Span<const game::UserEntityDefValues> udefs, rkit::Vector<rkit::ByteString> udefDescriptions);

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
		rkit::Span<const rkit::endian::LittleUInt32_t> entityTypes,
		rkit::Span<const uint8_t> spawnData, rkit::Vector<rkit::ByteStringView> spawnDefStrings,
		rkit::Span<const game::UserEntityDefValues> udefs, rkit::Vector<rkit::ByteString> udefDescriptions)
	{
		for (const rkit::endian::LittleUInt32_t &entityTypeLEU32 : entityTypes)
		{
			const uint32_t entityTypeID = entityTypeLEU32.Get();
			size_t spawnDataSize = 0;

			rkit::RCPtr<WorldObject> obj;
			void *fieldsRef = nullptr;
			SerializeFromLevelFunction_t deserializeFunc = nullptr;

			CORO_CHECK(WorldObjectFactory::CreateLevelObject(entityTypeID, spawnDataSize, obj, fieldsRef, deserializeFunc));

			if (!obj)
			{
				rkit::log::Error(u8"Entity type was invalid");
				CORO_THROW(rkit::ResultCode::kDataError);
			}

			if (spawnDataSize > spawnData.Count())
			{
				rkit::log::Error(u8"Spawn data was too small");
				CORO_THROW(rkit::ResultCode::kDataError);
			}

			WorldObjectSpawnParams spawnParams =
			{
				spawnDefStrings.ToSpan(),
				udefs,
				udefDescriptions.ToSpan()
			};

			CORO_CHECK(deserializeFunc(fieldsRef, spawnParams, spawnData.Ptr()));
			spawnData = spawnData.SubSpan(spawnDataSize);

			CORO_CHECK(world.AddObject(std::move(obj)));
		}

		if (spawnData.Count() > 0)
		{
			rkit::log::Error(u8"Extra spawn data was detected");
			CORO_THROW(rkit::ResultCode::kDataError);
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

	rkit::Result Session::AsyncSpawnInitialEntities(World &world,
		rkit::Span<const rkit::endian::LittleUInt32_t> entityTypes,
		rkit::Span<const uint8_t> spawnData,
		rkit::Vector<rkit::ByteStringView> spawnDefStrings,
		rkit::Span<const game::UserEntityDefValues> udefs,
		rkit::Vector<rkit::ByteString> udefDescriptions)
	{
		rkit::ICoroThread &thread = *Impl().m_mainCoroThread;
		return thread.EnterFunction(Impl().SpawnInitialEntities(thread, world, entityTypes, spawnData, std::move(spawnDefStrings), udefs, std::move(udefDescriptions)));
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
