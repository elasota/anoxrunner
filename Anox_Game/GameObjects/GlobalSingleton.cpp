#include "GlobalSingleton.h"

#include "GlobalSingleton.generated.inl"

#include "ScriptContext.h"
#include "ScriptManager.h"
#include "ScriptEnvironment.h"
#include "World.h"

namespace anox::game
{
	rkit::ResultCoroutine GlobalSingleton::OnSpawnedFromLevel(rkit::ICoroThread &thread)
	{
		CORO_CHECK(co_await GetWorld().GetScriptEnvironment().StartSequence(thread, GetScriptContext(), m_sequence, GetWorld()));

		CORO_RETURN_OK;
	}
}
