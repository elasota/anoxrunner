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

	class World final : public rkit::Opaque<WorldImpl>
	{
	public:
		explicit World(ScriptManager &scriptManager);

		rkit::Result AddObject(rkit::RCPtr<WorldObject> &&obj);

		rkit::WeakPtr<WorldObject> GetFirstObject() const;
		WorldObject* GetFirstObjectUnsafe() const;

		// Include AllWorldObjects.h for these
		AllWorldObjectsCollection GetAllObjects() const;

		rkit::ResultCoroutine OnWorldStarted(rkit::ICoroThread &thread);

		ScriptManager &GetScriptManager() const;
		ScriptEnvironment &GetScriptEnvironment() const;

		static rkit::Result Create(rkit::UniquePtr<World> &outWorld, ScriptManager &scriptManager);

	private:
		World() = delete;
	};
}
