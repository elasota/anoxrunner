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

	template<class T>
	class WeakPtr;

	struct ICoroThread;
}

namespace anox
{
	class AnoxSpawnDefsResourceBase;
}

namespace anox::game
{
	class WorldImpl;
	struct WorldObjectSpawnParams;
	class WorldObject;
	class AllWorldObjectsCollection;
	class AllWorldObjectsUnsafeCollection;
	class ScriptEnvironment;
	class ScriptManager;
	struct WorldObjectProxy;
	class MusicManager;
	class SceneManager;

	class World final : public rkit::Opaque<WorldImpl>
	{
	public:
		explicit World(ScriptManager &scriptManager);

		rkit::Result AddObject(rkit::RCPtr<WorldObjectProxy> &&obj);
		void RemoveObject(WorldObject *obj);

		// Include AllWorldObjects.h for these
		AllWorldObjectsCollection GetAllObjects() const;

		rkit::ResultCoroutine OnWorldStarted(rkit::ICoroThread &thread);
		rkit::ResultCoroutine OnRunFrame(rkit::ICoroThread &thread, uint64_t newGameTimeMSec);

		ScriptManager &GetScriptManager() const;
		ScriptEnvironment &GetScriptEnvironment() const;
		MusicManager &GetMusicManager() const;
		SceneManager &GetSceneManager() const;

		uint64_t GetCurrentTimeMSec() const;
		uint64_t GetPrevTimeMSec() const;
		uint16_t GetFrameDurationMSec() const;

		static rkit::Result Create(rkit::UniquePtr<World> &outWorld, ScriptManager &scriptManager);

	private:
		World() = delete;
	};
}

#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	inline uint16_t World::GetFrameDurationMSec() const
	{
		// Frame time deltas should be at most 100ms
		uint64_t timeDelta = this->GetCurrentTimeMSec() - this->GetPrevTimeMSec();
		RKIT_ASSERT(timeDelta <= std::numeric_limits<uint16_t>::max());
		return static_cast<uint16_t>(timeDelta);
	}
}
