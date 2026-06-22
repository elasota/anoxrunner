#include "WorldObject.h"

#include "ScriptContext.h"
#include "ScriptEnvironment.h"
#include "ScriptManager.h"
#include "World.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/RKitAssert.h"

namespace anox::game
{
	WorldObject::WorldObject()
	{
	}

	WorldObject::~WorldObject()
	{
	}

	rkit::Result WorldObject::Initialize(World &world)
	{
		m_world = &world;
		RKIT_CHECK(GetWorld().GetScriptEnvironment().CreateScriptContext(m_scriptContext));

		RKIT_RETURN_OK;
	}

	rkit::ResultCoroutine WorldObject::OnSpawnedFromLevel(rkit::ICoroThread &thread)
	{
		CORO_RETURN_OK;
	}

	rkit::ResultCoroutine WorldObject::OnFrame(rkit::ICoroThread &thread)
	{
		CORO_RETURN_OK;
	}
}
