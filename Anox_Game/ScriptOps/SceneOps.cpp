#include "APEExternDispatch.generated.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/LogDriver.h"

#include "rkit/Data/ContentID.h"

#include "SceneManager.h"
#include "ScriptEnvironment.h"
#include "ScriptManager.h"
#include "ScriptNamedResourceRef.h"
#include "World.h"

namespace anox::game::ape::externs
{
	rkit::ResultCoroutine loopscene::Execute(rkit::ICoroThread &thread, const ScriptExternContext &ctx, ExternDispatch::SceneResourceArg_t scene)
	{
		if (!scene.m_contentID)
		{
			rkit::log::ErrorFmt(u8"Failed to load scene {}", scene.m_name);
			CORO_RETURN_OK;
		}

		co_await ctx.m_world->GetSceneManager().RunScene(thread, scene.m_name, *scene.m_contentID, true);
		CORO_RETURN_OK;
	}
}
